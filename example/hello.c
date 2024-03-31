#include <stdlib.h>

#include <cJSON.h>

#include <dbs/event.h>
#include <dbs/log.h>

int hello(cJSON *data)
{
    char *string = cJSON_Print(data);
    print("hello, world!");
    print("hello data: %s", string);
    free(string);

    return 0;
}
declare_event(HELLO, hello);
