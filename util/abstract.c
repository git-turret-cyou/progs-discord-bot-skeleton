#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <dbs/abstract.h>
#include <dbs/api.h>
#include <dbs/init.h>
#include <dbs/log.h>
#include <dbs/util.h>

extern char *app_id;

int interaction_reply(cJSON *i, char *content, int raw) {
    struct curl_slist *headers = curl_slist_append(NULL, "Content-type: application/json");
    char *i_id = js_getStr(i, "id");
    char *i_token = js_getStr(i, "token");
    char *url = malloc(strlen("/interactions///callback") + strlen(i_id) + strlen(i_token));
    long code;

    char *packet = NULL;

    cJSON *packet_js = cJSON_CreateObject();
    cJSON_AddNumberToObject(packet_js, "type", 4);

    if(raw) {
        cJSON_AddItemToObject(packet_js, "data", cJSON_Parse(content));
    } else {
        cJSON *message_js = cJSON_CreateObject();
        cJSON_AddStringToObject(message_js, "content", content);
        cJSON_AddItemToObject(packet_js, "data", message_js);
    }

    packet = cJSON_PrintUnformatted(packet_js);
    cJSON_Delete(packet_js);

    sprintf(url, "/interactions/%s/%s/callback", i_id, i_token);

    close(api_patch(url, headers, packet, &code));
    free(url);
    free(packet);
    curl_slist_free_all(headers);

    return (code % 100) != 200;
}

int interaction_defer_reply(cJSON *i, int hidden) {
    struct curl_slist *headers = curl_slist_append(NULL, "Content-type: application/json");
    char *i_id = js_getStr(i, "id");
    char *i_token = js_getStr(i, "token");
    char *url = malloc(strlen("/interactions///callback") + strlen(i_id) + strlen(i_token) + 2);
    long code;

    char *packet = NULL;

    cJSON *packet_js = cJSON_CreateObject();
    cJSON_AddNumberToObject(packet_js, "type", 5);
    if(hidden) {
        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "flags", 1 << 6);
        cJSON_AddItemToObject(packet_js, "data", data);
    }
    packet = cJSON_PrintUnformatted(packet_js);
    cJSON_Delete(packet_js);

    sprintf(url, "/interactions/%s/%s/callback", i_id, i_token);

    close(api_post(url, headers, packet, &code));

    free(url);
    free(packet);
    curl_slist_free_all(headers);

    return (code % 100) != 200;
}

int interaction_edit_reply(cJSON *i, char *content, int raw) {
    struct curl_slist *headers = curl_slist_append(NULL, "Content-type: application/json");
    char *i_token = js_getStr(i, "token");
    char *url = malloc(strlen("/webhooks///messages/@original") + strlen(app_id) + strlen(i_token) + 2);
    long code;

    char *message = content;

    if(!raw) {
        cJSON *message_js = cJSON_CreateObject();
        cJSON_AddStringToObject(message_js, "content", content);
        message = cJSON_PrintUnformatted(message_js);
        cJSON_Delete(message_js);
    }

    sprintf(url, "/webhooks/%s/%s/messages/@original", app_id, i_token);

    close(api_patch(url, headers, message, &code));
    free(url);
    if(!raw) free(message);
    curl_slist_free_all(headers);

    return (code % 100) != 200;
}
