#define LOG_SOH "\001"
#define LOG_SOH_ASCII '\001'

#define   EMERG_LOGLEVEL 0
#define   ALERT_LOGLEVEL 1
#define    CRIT_LOGLEVEL 2
#define     ERR_LOGLEVEL 3
#define WARNING_LOGLEVEL 4
#define  NOTICE_LOGLEVEL 5
#define    INFO_LOGLEVEL 6
#define   DEBUG_LOGLEVEL 7

#define DEFAULT_LOGLEVEL NOTICE_LOGLEVEL
#define CONSOLE_LOGLEVEL DEBUG_LOGLEVEL

#define LOG_EMERG LOG_SOH "\1" "0"
#define LOG_ALERT LOG_SOH "\1" "1"
#define LOG_CRIT LOG_SOH "\1" "2"
#define LOG_ERR LOG_SOH "\1" "3"
#define LOG_WARNING LOG_SOH "\1" "4"
#define LOG_NOTICE LOG_SOH "\1" "5"
#define LOG_INFO LOG_SOH "\1" "6"
#define LOG_DEBUG LOG_SOH "\1" "7"

#define LOG_DEFAULT ""

int print(const char *fmt, ...);

#define PANICMODE_DEBUGONLY 'o'
#define PANICMODE_RESPAWN 'r'
#define PANICMODE_DIE 'd'

#define PANIC_OOPS LOG_SOH #PANICMODE_DEBUGONLY
#define PANIC_RESPAWN LOG_SOH #PANICMODE_RESPAWN
#define PANIC_PANIC LOG_SOH #PANICMODE_DIE

#define PANIC_DEFAULT PANIC_PANIC

void _panic(const char *fileorigin, const int lineorigin, const char *fmt, ...);
#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define oops(...) _panic(__FILE__, __LINE__, PANIC_OOPS __VA_ARGS__)

#define ANSI_CSI "\x1b["

#define ANSI_BOLD ANSI_CSI "1m"
#define ANSI_ITALIC ANSI_CSI "3m"
#define ANSI_BLINK ANSI_CSI "5m"
#define ANSI_REVERSE ANSI_CSI "7m"
#define ANSI_RESET ANSI_CSI "0m"

#define ANSI_BLACK ANSI_CSI "30m"
#define ANSI_RED ANSI_CSI "31m"
#define ANSI_GREEN ANSI_CSI "32m"
#define ANSI_YELLOW ANSI_CSI "33m"
#define ANSI_BLUE ANSI_CSI "34m"
#define ANSI_MAGENTA ANSI_CSI "35m"
#define ANSI_CYAN ANSI_CSI "36m"
#define ANSI_WHITE ANSI_CSI "37m"

#define ANSI_BRIGHT_BLACK ANSI_CSI "90m"
#define ANSI_BRIGHT_RED ANSI_CSI "91m"
#define ANSI_BRIGHT_GREEN ANSI_CSI "92m"
#define ANSI_BRIGHT_YELLOW ANSI_CSI "93m"
#define ANSI_BRIGHT_BLUE ANSI_CSI "94m"
#define ANSI_BRIGHT_MAGENTA ANSI_CSI "95m"
#define ANSI_BRIGHT_CYAN ANSI_CSI "96m"
#define ANSI_BRIGHT_WHITE ANSI_CSI "97m"
