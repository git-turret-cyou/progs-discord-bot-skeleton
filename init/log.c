#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>

#include <log.h>
#include <util.h>

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

int start_time = 0;
int console_lock = 0;

int print(const char *fmt, ...)
{
    int loglevel = DEFAULT_LOGLEVEL;
    if(fmt[0] == LOG_SOH_ASCII) {
        loglevel = (fmt[1] - 0x30) % 10;
        fmt += 2;
    }

    /* not going to be printed? dont bother! */
    if(loglevel > CONSOLE_LOGLEVEL)
        return 0;

    /* we essentially print the user's raw input to its own buffer,
       later we will parse it and print out ANSI colors and what not */
    char buf[512];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 512, fmt, ap);
    va_end(ap);
    buf[512 - 1] = '\0';

    size_t colon = 0;
    for(; colon < strlen(buf); ++colon) {
        if(buf[colon] == ':')
            break;
    }

    char tsbuf[64] = "\0";
    struct timeval time;
    gettimeofday(&time, NULL);
    snprintf(tsbuf, sizeof(tsbuf), "[%5ld.%06ld] ", (long)time.tv_sec % 100000, (long)time.tv_usec);

    /* spin lock, at the cost of architecture portability
       concurrency is something that we need to adjust for, and the
       console will be scrambled and unreadable if we allow writing all
       at the same time. I considered simply writing all at once, but
       ended up just not caring enough to the point where spinlocks
       prevail. */
    __asm__(".spin_lock:");
    __asm__("mov rax, 1");
    __asm__("xchg rax, [console_lock]");
    __asm__("test rax, rax");
    __asm__("jnz .spin_lock");

    /* we want to support stuff without colons, but frankly I havent
       tested this at time of writing. will find out later */
    writeputs(ANSI_RESET ANSI_GREEN);
    writeputs(tsbuf);
    writeputs(ANSI_RESET);
    if(buf[colon] == ':') {
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

    console_lock = 0;
    return 0;
}
