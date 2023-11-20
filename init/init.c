#include <signal.h>
#include <sys/wait.h>

#include <init.h>
#include <util.h>

/* For some reason, I get SIGSEGV'd when running because a random-ass
   byte was inserted where it isnt supposed to be. Added a safety byte
   because I cannot be asked to try to figure out how to do this cleanly. */
static unsigned long __1bsafebuf
    __attribute__((used)) __attribute__((section(".1bsafebuf.init"))) = 0;

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

    for (fn = initcall_levels[level - 1]; fn < initcall_levels[level]; fn++)
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
    do_initcalls();

    static sigset_t set;
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, NULL);

    while(1) {
        int sig;
        sigwait(&set, &sig);
        if(sig == SIGCHLD)
            while(waitpid(0, NULL, WNOHANG) > 0)
                ;
    }
}
