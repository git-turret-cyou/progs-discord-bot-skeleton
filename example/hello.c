#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <dbs/event.h>
#include <dbs/log.h>
#include <dbs/util.h>

extern CURL *ws_handle;
char *app_id;

int hello(cJSON *data)
{
    (void)(data);
    print("hello: hello from userland!");

    return 0;
}
declare_event(HELLO, hello);

int ready(cJSON *data)
{
    print("hello: received ready event!");

    cJSON *app = cJSON_GetObjectItemCaseSensitive(data, "application");
    char *id = js_getStr(app, "id");
    app_id = malloc(strlen(id) + 1);
    strcpy(app_id, id);

    cJSON *user = cJSON_GetObjectItemCaseSensitive(data, "user");
    char *username = js_getStr(user, "username");
    print("hello: logged in! my name is %s (id %s)", username, app_id);

    return 0;
}
declare_event(READY, ready);
