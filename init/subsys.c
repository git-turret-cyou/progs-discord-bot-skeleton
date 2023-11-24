#include <errno.h>
#include <sched.h>
#include <signal.h>
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
#define MAX_SUBSYSTEMS 32

struct subsystem_info {
    char *fn_name;
    int (*fn)(void);
    int pid;
    void *stack;
};

extern int mainpid;
static struct subsystem_info *subsystems[MAX_SUBSYSTEMS + 1];
static int subsystem_count = 0;

static int __subsystem_entry(struct subsystem_info *info)
{
    /* entry point from clone(). we setup the process name so we know
       what we are looking at from a glance in a ps view or htop or
       whatever. */
    char *name = malloc(16 * sizeof(char));
    snprintf(name, 16, NAME_SHORTHAND ": %s", info->fn_name);
    name[15] = '\0';
    prctl(PR_SET_NAME, name);
    free(name);

    print(LOG_DEBUG "subsys: starting subsystem %s (%d)", info->fn_name, getpid());

    int ret = info->fn();

    return ret;
}

int subsystem_handle_term(int pid)
{
    for(int i = 0; i < MAX_SUBSYSTEMS; ++i) {
        struct subsystem_info *subsystem = subsystems[i];
        if(!subsystem || subsystem->pid != pid)
            continue;

        print(LOG_DEBUG "subsys: subsystem terminated %s (%d)", subsystem->fn_name, pid);

        if(munmap(subsystem->stack, STACK_SIZE) < 0)
            print(LOG_EMERG "subsys: failed to deallocate stack for subsystem %s (%d) (errno %d)", subsystem->fn_name, pid, errno);
        subsystems[i] = 0;
        free(subsystem);

        return 0;
    }

    return 1;
}

int __impl_start_subsystem(char *fn_name, int (*fn)(void))
{
    if(getpid() != mainpid) {
        print(LOG_CRIT "subsys: cannot perform subsystem inception (attempted from %d)", getpid());
        return 1;
    }
    if(subsystem_count >= MAX_SUBSYSTEMS) {
        print(LOG_CRIT "subsys: cannot start subsystem %s: reached maximum number of subsystems", fn_name);
        return 1;
    }

    /* because CLONE_VM is being set, our stack is not duplicated and
       therefore we need to map a stack */
    void *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK | MAP_PRIVATE, -1, 0);
    if((long)stack <= 0) {
        print(LOG_CRIT "subsys: cannot start subsystem %s: failed to allocate stack (errno %d)", fn_name, errno);
        return 1;
    }

    /* the libc gods have graced us with the ability to pass one (1) arg
       to the function. struct required. the absence of a free is not a
       memory leak because we free it above. */
    struct subsystem_info *info = malloc(sizeof(struct subsystem_info));
    info->fn_name = fn_name;
    info->fn = fn;
    info->stack = stack;

    int pid = clone((int (*)(void *))__subsystem_entry, (void *)((long)stack + STACK_SIZE), CLONE_FILES | CLONE_VM | SIGCHLD, info);
    info->pid = pid;
    if(pid < 0) {
        print(LOG_CRIT "subsys: cannot start subsystem %s: clone failed (errno %d)", fn_name, errno);
        munmap(stack, STACK_SIZE);
        free(info);
        return 1;
    }

    for(int i = 0; i < MAX_SUBSYSTEMS; ++i) {
        if(!subsystems[i]) {
            subsystems[i] = info;
            ++subsystem_count;
            break;
        }
    }

    return 0;
}
