#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define writeputs(str) write(STDOUT_FILENO, str, strlen(str));
