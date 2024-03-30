#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <curl/curl.h>

#include <dbs/init.h>
#include <dbs/log.h>
#include <dbs/util.h>

extern int subsystem_handle_term(int pid);
extern int subsystem_count;
int mainpid = 0;
long stack_size = 8192 * 512;

/* For some reason, I get SIGSEGV'd when running because a random-ass
   byte was inserted where it isnt supposed to be. Added a safety byte
   because I cannot be asked to try to figure out how to do this cleanly. */
static unsigned long __1bsafebuf
    __attribute__((used)) __attribute__((section(".1bsafebuf.init"))) = 0;

/* We start initcall levels at [1] instead of [0], so we must adjust
   in code for this minor design choice. Math is done on the level passed
   through i.e. do_initcall_level so that you can call it with (1) and have
   the expected initcall (l1_initcall) run. */
extern initcall_entry_t __initcall1_start[];
extern initcall_entry_t __initcall2_start[];
extern initcall_entry_t __initcall3_start[];
extern initcall_entry_t __initcall4_start[];
extern initcall_entry_t __initcall5_start[];
extern initcall_entry_t __initcall_end[];

static initcall_entry_t *initcall_levels[] = {
    __initcall1_start,
    __initcall2_start,
    __initcall3_start,
    __initcall4_start,
    __initcall5_start,
    __initcall_end,
};

static void do_initcall_level(int level)
{
    initcall_entry_t *fn;

    for (fn = initcall_levels[level - 1];
            fn < initcall_levels[level];
            fn++)
        initcall_from_entry(fn)();
}

static void do_initcalls(void)
{
    unsigned long level;
    for (level = 1; level < ARRAY_SIZE(initcall_levels); level++) {
        do_initcall_level(level);
    }
}

static void doenv(char *path)
{
    int fd = open(path, O_RDONLY);
    if(fd < 0)
        return;

    struct stat statbuf;
    if(fstat(fd, &statbuf) < 0)
        return;

    char *file_mmap = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(file_mmap == NULL)
        return;

    char *file = malloc(statbuf.st_size + 1);
    file[statbuf.st_size + 1] = 0;
    memcpy(file, file_mmap, statbuf.st_size);
    munmap(file_mmap, statbuf.st_size);

    int offset = 0;
    while(1) {
        char *line = &(file[offset]);
        if(*line == '\0')
            break;

        char *eol = strchrnul(line, '\n');
        *eol = '\0';
        if(*line == '#')
            goto nextline;

        char *divider = strchr(line, '=');
        if(divider == NULL)
            goto nextline;

        *divider = '\0';
        setenv(line, divider + 1, 0);

nextline:
        offset += (eol - line) + 1;
        continue;
    }

    free(file);
}

int main(void)
{
    /* Hello, World! */

    /* set mainpid for the subsystem service so it is fully accessible
       during l1 */
    mainpid = getpid();

    /* set stack_size for subsystem service */
    struct rlimit *stack_rlimit = malloc(sizeof(struct rlimit));
    getrlimit(RLIMIT_STACK, stack_rlimit);
    if(stack_rlimit->rlim_cur != RLIM_INFINITY) {
        stack_size = MIN(stack_rlimit->rlim_cur, stack_size);
    }
    free(stack_rlimit);

    /* configure signal handlers early to prevent race condition where subsystems
       can terminate main process on accident, and disable Terminated output during
       early-mode panic */
    static sigset_t set;
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, NULL);

    /* use .env files if present */
    doenv(".env");

    /* find directory of self and use env from there if it exists */
    {
        char *buf = calloc(PATH_MAX, sizeof(char));
        ssize_t self_size = readlink("/proc/self/exe", buf, PATH_MAX);
        if(self_size + strlen(".env") + 1 > PATH_MAX)
            goto skip_self;

        char *lastslash = strrchr(buf, '/');
        *lastslash = '\0';

        char *cwd = get_current_dir_name();
        int cwd_is_exec_dir = strcmp(buf, cwd) == 0;
        free(cwd);
        if(cwd_is_exec_dir)
            goto skip_self;

        strcat(buf, "/.env");
        doenv(buf);
skip_self:
        free(buf);
    }

    /* init curl */
    if(curl_global_init(CURL_GLOBAL_DEFAULT))
        panic("init: curl init failed");

    /* init random seed */
    srand(time(NULL));

    /* Perform initcalls */
    do_initcalls();

    /* Reaper. Much like init. */

    siginfo_t siginfo;
    while(subsystem_count > 0) {
        sigwaitinfo(&set, &siginfo);
        int sig = siginfo.si_signo;
        switch(sig) {
        case SIGCHLD: ;
            int process = 0;
            while((process = waitpid(-1, NULL, WNOHANG)) > 0)
                if(subsystem_handle_term(process) > 0)
                    print(LOG_WARNING "init: failed to reap process %d",
                            process);
            if(siginfo.si_status != 0) {
                panic("init: process %d exited with non-zero status (%d)", siginfo.si_pid, siginfo.si_status);
            }
            break;
        case SIGINT:
            panic("init: keyboard interrupt");
            break;
        case SIGTERM:
            exit(0);
            break;
        default:
            break;
        }
    }

    panic("init: no more subsystems");
}
