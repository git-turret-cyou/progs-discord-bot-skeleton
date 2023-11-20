#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>

#include <config.h>
#include <log.h>
#include <util.h>

#define STACK_SIZE 8192 * 512

struct subsystem_info {
    char *fn_name;
    int (*fn)(void);
};

int __subsystem_entry(struct subsystem_info *info)
{
    /* entry point from clone(). we setup the process name so we know
       what we are looking at from a glance in a ps view or htop or
       whatever. */
    char *name = malloc(16 * sizeof(char));
    snprintf(name, 16, NAME_SHORTHAND ": %s", info->fn_name);
    name[15] = '\0';
    prctl(PR_SET_NAME, name);
    free(name);

    int ret = info->fn();

    /* we are expected to free info before  exiting */
    free(info);

    return ret;
}

int __impl_start_subsystem(char *fn_name, int (*fn)(void))
{
    /* because CLONE_VM is being set, our stack is not duplicated and
       therefore we need to map a stack */
    void *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK | MAP_PRIVATE, -1, 0);
    if((long)stack < 0)
        die("subsys mmap:");

    /* the libc gods have graced us with the ability to pass one (1) arg
       to the function. struct required. the absence of a free is not a
       memory leak because we free it above. */
    struct subsystem_info *info = malloc(sizeof(struct subsystem_info));
    info->fn_name = fn_name;
    info->fn = fn;

    int pid = clone((int (*)(void *))__subsystem_entry, (void *)((long)stack + STACK_SIZE), CLONE_FILES | CLONE_VM, info);
    if(pid < 0) {
        munmap(stack, STACK_SIZE);
        die("subsys clone:");
    }

    return 0;
}
