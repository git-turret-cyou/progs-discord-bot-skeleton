#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <curl/curl.h>

#include <config.h>
#include <init.h>
#include <log.h>
#include <util.h>

extern int subsystem_handle_term(int pid);
extern int subsystem_count;
int mainpid = 0;
long stack_size = 8192 * 512;
char *token;

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

int main(void)
{
    print("init: Hello world! Running " NAME " v" VERSION "!");

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

    /* fetch token */
    char *token_base = getenv("TOKEN");
    if(!token_base)
        panic("init: cannot find TOKEN in env");

    token = calloc(strlen(token_base) + strlen("Authorization: Bot ") + 1,
            sizeof(char));
    strcpy(token, "Authorization: Bot ");
    strcat(token, token_base);

    /* init curl */
    if(curl_global_init(CURL_GLOBAL_DEFAULT))
        panic("init: curl init failed");

    /* Rest of the program.. */
    do_initcalls();

    /* Reaper. Much like init. */
    siginfo_t siginfo;
    static sigset_t set;
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, NULL);

    while(subsystem_count > 0) {
        sigwaitinfo(&set, &siginfo);
        int sig = siginfo.si_signo;
        if(sig == SIGCHLD) {
            int process = 0;
            while((process = waitpid(-1, NULL, WNOHANG)) > 0)
                if(subsystem_handle_term(process) > 0)
                    print(LOG_WARNING "init: failed to reap process %d",
                            process);
            if(siginfo.si_status != 0) {
                panic("init: process %d exited with non-zero status (%d)", siginfo.si_status);
            }
        } else if(sig == SIGINT) {
            panic("init: keyboard interrupt");
        }
    }

    panic("init: no more subsystems");
}
