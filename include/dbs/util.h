#ifndef __UTIL_H
#define __UTIL_H

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define writeputs(str) write(STDOUT_FILENO, str, strlen(str));

#ifdef cJSON__h

static inline int js_getInt(cJSON *js, char *name)
{
    cJSON *number = cJSON_GetObjectItemCaseSensitive(js, name);
    if(cJSON_IsNumber(number))
        return number->valueint;

    return -1;
}

static inline char *js_getStr(cJSON *js, char *name)
{
    cJSON *str = cJSON_GetObjectItemCaseSensitive(js, name);
    if(cJSON_IsString(str))
        return str->valuestring;
    return NULL;
}

#endif

#endif
