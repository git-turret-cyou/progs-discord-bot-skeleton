#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void die(const char *fmt, ...);

#define writeputs(str) write(STDOUT_FILENO, str, strlen(str));
