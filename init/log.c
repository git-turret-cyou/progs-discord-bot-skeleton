#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include <log.h>
#include <util.h>

static const char *colors[] = {
    [0] = ANSI_BLINK ANSI_REVERSE ANSI_BOLD ANSI_RED,
    [1] = ANSI_REVERSE ANSI_BOLD ANSI_RED,
    [2] = ANSI_BOLD ANSI_RED,
    [3] = ANSI_RED,
    [4] = ANSI_BOLD,
    [5] = ANSI_BRIGHT_WHITE,
    [6] = ANSI_RESET,
    [7] = ANSI_ITALIC ANSI_BRIGHT_BLUE,
};

int console_lock = 0;

int print(const char *fmt, ...)
{
    int loglevel = 5;
    if(fmt[0] == LOG_SOH_ASCII) {
        loglevel = (fmt[1] - 0x30) % 10;
        fmt += 2;
    }

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

    /* spin lock, at the cost of architecture portability */
    __asm__(".spin_lock:");
    __asm__("mov rax, 1");
    __asm__("xchg rax, [console_lock]");
    __asm__("test rax, rax");
    __asm__("jnz .spin_lock");

    if(buf[colon] == ':') {
        writeputs(ANSI_RESET);
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

    console_lock = 0;
    return 0;
}
