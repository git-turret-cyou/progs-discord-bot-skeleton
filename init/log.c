#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#include <log.h>
#include <util.h>

extern int subsystem_change_mode(int pid, char mode);
extern char *subsystem_get_name(int pid);
extern int mainpid;

static const char *colors[] = {
    [EMERG_LOGLEVEL] = ANSI_BLINK ANSI_REVERSE ANSI_BOLD ANSI_RED,
    [ALERT_LOGLEVEL] = ANSI_REVERSE ANSI_BOLD ANSI_RED,
    [CRIT_LOGLEVEL] = ANSI_BOLD ANSI_RED,
    [ERR_LOGLEVEL] = ANSI_RED,
    [WARNING_LOGLEVEL] = ANSI_BOLD,
    [NOTICE_LOGLEVEL] = ANSI_BRIGHT_WHITE,
    [INFO_LOGLEVEL] = ANSI_RESET,
    [DEBUG_LOGLEVEL] = ANSI_ITALIC ANSI_BRIGHT_BLUE,
};

static const char *mode_to_string[] = {
    [PANICMODE_DEBUGONLY] = "subsystem OOPS",
    [PANICMODE_RESPAWN] = "subsystem failure",
    [PANICMODE_DIE] = "catastrophic failure",
};

static int console_lock = 0;

#define MAX_TRY_COUNT 1 << 17
static void obtain_console_lock(void)
{
    int try_count = 0;
    register int rax asm("rax");
retry:
    while(console_lock && try_count <= MAX_TRY_COUNT)
        try_count += 1;

    asm("mov %0, 1 \n"
        "xchg %0, %1" : "=r" (rax), "=m" (console_lock));

    if(rax > 0 && try_count <= MAX_TRY_COUNT)
        goto retry;

    if(try_count > MAX_TRY_COUNT) {
        print(LOG_SOH "\3" "4" "log: broken console lock");
    }

    return;
}

static int vaprint(const char *fmt, va_list ap)
{
    int loglevel = DEFAULT_LOGLEVEL;
    int dolocks = 1;
    int parsecolon = 1;
    if(fmt[0] == LOG_SOH_ASCII) {
        loglevel = (fmt[2] - 0x30) % 10;
        char flags = fmt[1];
        if(flags & 1 << 1)
            dolocks = 0;
        if(flags & 1 << 2)
            parsecolon = 0;
        fmt += 3;
    }

    /* not going to be printed? dont bother! */
    if(loglevel > CONSOLE_LOGLEVEL)
        return 0;

    /* we essentially print the user's raw input to its own buffer,
       later we will parse it and print out ANSI colors and what not */
    char buf[512];

    vsnprintf(buf, 512, fmt, ap);
    buf[512 - 1] = '\0';

    size_t colon = 0;
    if(parsecolon) {
        for(; colon < strlen(buf); ++colon) {
            if(buf[colon] == ':')
                break;
        }
    }

    char tsbuf[64] = "\0";
    struct timeval time;
    gettimeofday(&time, NULL);
    snprintf(tsbuf, sizeof(tsbuf), "[%5ld.%06ld] ",
            (long)time.tv_sec % 100000, (long)time.tv_usec);

    /* spin lock, at the cost of architecture portability
       concurrency is something that we need to adjust for, and the
       console will be scrambled and unreadable if we allow writing all
       at the same time. I considered simply writing all at once, but
       ended up just not caring enough to the point where spinlocks
       prevail. */
    if(dolocks)
        obtain_console_lock();


    /* we want to support stuff without colons, but frankly I havent
       tested this at time of writing. will find out later */
    writeputs(ANSI_RESET ANSI_GREEN);
    writeputs(tsbuf);
    writeputs(ANSI_RESET);
    if(parsecolon && buf[colon] == ':') {
        writeputs(colors[loglevel]);
        writeputs(ANSI_YELLOW);
        write(STDOUT_FILENO, buf, colon);
        writeputs(ANSI_RESET);
    }
    writeputs(colors[loglevel]);
    if(colon && *(buf + colon)) {
        writeputs(buf + colon);
    } else {
        writeputs(buf);
    }
    writeputs(ANSI_RESET);
    write(STDOUT_FILENO, "\n", 1);
    if(dolocks)
        console_lock = 0;
    return 0;
}

void _panic(const char *fileorigin,
        const int lineorigin,
        const char *fmt, ...)
{
    char mode = PANICMODE_DIE;
    int pid = getpid();
    if(fmt[0] == LOG_SOH_ASCII) {
        mode = fmt[1];
        /* cannot respawn main thread */
        if(pid == mainpid && mode == PANICMODE_RESPAWN)
            mode = PANICMODE_DIE;
        fmt += 2;
    }

#define NOLOCK(loglevel) LOG_SOH "\3" loglevel
    va_list ap;
    va_start(ap, fmt);
    char *_fmt = malloc(strlen(fmt) + 4 * sizeof(char));
    sprintf(_fmt, NOLOCK("1") "%s", fmt);

    void **backtrace_addresses = malloc(sizeof(void*) * 32);
    int backtrace_count = backtrace(backtrace_addresses, 32);
    char **backtrace_symbolnames =
        backtrace_symbols(backtrace_addresses, backtrace_count);

    obtain_console_lock();

    print(NOLOCK("5") "------------[ cut here ]------------");
    print(LOG_SOH "\7""0"  "%s at %s:%d", mode_to_string[(int)mode],
            fileorigin, lineorigin);
    vaprint(_fmt, ap);
    print(LOG_SOH "\7""7" "Call Trace:");
    for(int i = 0; i < backtrace_count; ++i) {
        print(NOLOCK("7") " [0x%016x]  %s", backtrace_addresses[i],
                backtrace_symbolnames[i]);
    }
    if(mainpid == pid){
        print(NOLOCK("7") "                       <start of main thread>");
    } else {
        print(NOLOCK("7") "                       <start of %s[%d]>",
                subsystem_get_name(pid), pid);
    }

    /* if we are going to die, we dont really need to clean up */
    if(mode == PANICMODE_DIE) {
        kill(0, SIGTERM);
        raise(SIGTERM);
        exit(0);
    }

    print(NOLOCK("5") "------------[ cut here ]------------");

    console_lock = 0;
    free(_fmt);
    free(backtrace_symbolnames);
    free(backtrace_addresses);
    va_end(ap);

    if(mode == PANICMODE_DEBUGONLY)
        return;

    if(pid != mainpid && mode == PANICMODE_RESPAWN) {
        /* we want to let the main process handle the rest */
        subsystem_change_mode(pid, mode);
        syscall(SYS_exit_group, 0);
    }
}

int print(const char *fmt, ...)
{
    int ret = 0;
    va_list ap;
    va_start(ap, fmt);
    ret = vaprint(fmt, ap);
    va_end(ap);
    return ret;
}
