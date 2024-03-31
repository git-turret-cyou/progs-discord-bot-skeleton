#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <dbs/event.h>
#include <dbs/log.h>

extern CURL *ws_handle;

int hello(cJSON *data)
{
    cJSON *ev_payload = cJSON_CreateObject();
    cJSON *ev_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(ev_payload, "op", 2);
    cJSON_AddItemToObject(ev_payload, "d", ev_data);

    cJSON_AddStringToObject(ev_data, "token", getenv("TOKEN"));
    cJSON_AddNumberToObject(ev_data, "intents", 0);

    cJSON *properties = cJSON_CreateObject();
    cJSON_AddItemToObject(ev_data, "properties", properties);
    cJSON_AddStringToObject(properties, "browser", "DBS");
    cJSON_AddStringToObject(properties, "device", "DBS");

    struct utsname unamed;
    uname(&unamed);
    cJSON_AddStringToObject(properties, "os", unamed.sysname);

    char *msg = cJSON_PrintUnformatted(ev_payload);
    size_t sent;
    curl_ws_send(ws_handle, msg, strlen(msg), &sent, 0, CURLWS_TEXT);
    free(msg);
    cJSON_Delete(ev_payload);
    print("hello: sent IDENT");

    return 0;
}
declare_event(HELLO, hello);
