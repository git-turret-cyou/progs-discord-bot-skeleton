#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <cJSON.h>

#include <dbs/api.h>
#include <dbs/event.h>
#include <dbs/init.h>
#include <dbs/log.h>
#include <dbs/util.h>

extern char *app_id;
extern double api_latency;

static void ping(cJSON *i)
{
    struct curl_slist *headers = curl_slist_append(NULL, "Content-type: application/json");
    headers = curl_slist_append(headers, "User-Agent: DiscordBot (DBS, dev)");

    char *i_id = js_getStr(i, "id");
    char *i_token = js_getStr(i, "token");
    char *url = malloc(strlen("/interactions/") + strlen(i_id) + strlen("/") + strlen(i_token) + strlen("/messages/@original") + 2);

    sprintf(url, "/interactions/%s/%s/callback", i_id, i_token);
    char *defer_message = "{\"type\":5}";

    struct timeval sent;
    struct timeval done;

    long code;
    gettimeofday(&sent, NULL);
    close(api_post(url, headers, defer_message, &code));
    gettimeofday(&done, NULL);

    char *response = malloc(128);
    snprintf(response, 128, "{\"embeds\":[{\"title\": \"PONG!\", \"description\": \"Measured API Latency: %.4fms\\nWS Latency: %.4fms\"}]}",
            (done.tv_sec - sent.tv_sec) * 1000.0f + (done.tv_usec - sent.tv_usec) / 1000.0f,
            api_latency);
    sprintf(url, "/webhooks/%s/%s/messages/@original", app_id, i_token);
    headers = curl_slist_append(NULL, "Content-type: application/json");
    headers = curl_slist_append(headers, "User-Agent: DiscordBot (DBS, dev)");

    close(api_patch(url, headers, response, &code));

    free(response);
    free(url);
}

static void hi(cJSON *i)
{

    char *i_id = js_getStr(i, "id");
    char *i_token = js_getStr(i, "token");
    char *url = malloc(strlen("/interactions/") + strlen(i_id) + strlen("/") + strlen(i_token) + strlen("/callback") + 2);
    sprintf(url, "/interactions/%s/%s/callback", i_id, i_token);

    char *message = "{\"type\": 4,\"data\":{\"type\":4,\"flags\": 0,\"content\":\"hello world!\"}}";
    long code;

    struct curl_slist *headers = curl_slist_append(NULL, "Content-type: application/json");
    headers = curl_slist_append(headers, "User-Agent: DiscordBot (DBS, dev)");
    close(api_post(url, headers, message, &code));

    free(url);
}

int interaction_create(cJSON *ev_data)
{
    int i_type = js_getInt(ev_data, "type");
    cJSON *i = cJSON_GetObjectItemCaseSensitive(ev_data, "data");

    switch(i_type) {
    case 1: ; /* PING */
        break;
    case 2: ; /* APPLICATION_COMMAND */
        int cmd_type = js_getInt(i, "type");
        if(cmd_type == -1) cmd_type = 1;
        char *cmd_name = js_getStr(i, "name");
        if(strcmp(cmd_name, "ping") == 0) {
            ping(ev_data);
            return 0;
        } else if(strcmp(cmd_name, "hi") == 0) {
            hi(ev_data);
            return 0;
        }
        break;
    case 3: ; /* MESSAGE_COMPONENT */
        break;
    case 4: ; /* APPLICATION_COMMAND_AUTOCOMPLETE */
        break;
    case 5: ; /* MODAL_SUBMIT */
        break;
    default:
        break;
    }

    char *payload = cJSON_Print(ev_data);
    print("inter_create: payload (see below)\n%s", payload);
    free(payload);

    return 1;
}
declare_event(INTERACTION_CREATE, interaction_create);
