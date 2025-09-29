#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>
#include <cJSON.h>
#include <stb_ds.h>

#include <dbs/abstract.h>
#include <dbs/api.h>
#include <dbs/commands.h>
#include <dbs/event.h>
#include <dbs/init.h>
#include <dbs/log.h>
#include <dbs/util.h>

extern char *app_id;
extern double api_latency;

Command ping_command = {
    .type = COMMAND_CHAT_INPUT,
    .name = "ping",
    .description = "ping pong all the way to america :flag_us:",
    .options = NULL
};
void ping(cJSON *i)
{
    static int do_hidden = 1;
    struct timeval sent;
    struct timeval done;
    gettimeofday(&sent, NULL);
    if(!interaction_defer_reply(i, do_hidden))
        print("ping: DEFER failed");
    do_hidden = !do_hidden;
    gettimeofday(&done, NULL);

    char *response = malloc(128);
    snprintf(response, 128, "{\"embeds\":[{\"title\": \"PONG!\", \"description\": \"Measured API Latency: %.4fms\\nWS Latency: %.4fms\"}]}",
            (done.tv_sec - sent.tv_sec) * 1000.0f + (done.tv_usec - sent.tv_usec) / 1000.0f,
            api_latency);
    if(!interaction_edit_reply(i, response, 1))
        print("ping: EDIT failed");
    free(response);
}
declare_command(ping);

Command hi_command = {
    .type = COMMAND_CHAT_INPUT,
    .name = "hi",
    .description = "hallo!!!!",
    .options = NULL
};
static void hi(cJSON *i)
{
    if(!interaction_reply(i, "hello world!", 0))
        print("hi: REPLY failed");
}
declare_command(hi);

int interaction_create(cJSON *ev_data)
{
    int i_type = js_getInt(ev_data, "type");
    cJSON *i = cJSON_GetObjectItemCaseSensitive(ev_data, "data");

    switch(i_type) {
    case 1: ; /* PING */
        break;
    case 2: ; /* APPLICATION_COMMAND */
        char *cmd_name = js_getStr(i, "name");
        for(int i = 0; i < arrlen(commands); ++i) {
            if(strcmp(cmd_name, commands[i].name) == 0) {
                commands[i].callback(ev_data);
                return 0;
            }
        }
        print(LOG_WARN "commands: unknown command %s", cmd_name);
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
