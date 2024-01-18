#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <log.h>

extern CURL *ws_handle;
long last_sequence = -1;
struct timeval heartbeat_time;

void ws_send_heartbeat()
{
    char buf[128] = "{\"op\":1,\"d\":null}";
    if(last_sequence > 0)
        snprintf(buf, 128, "{\"op\":1,\"d\":%ld}", last_sequence);
    size_t sent;
    curl_ws_send(ws_handle, buf, strnlen(buf, 128), &sent, 0, CURLWS_TEXT);

    /* if we receive a heartbeat request from discord, we need to fix
       the itimer so we don't send another one before the desired
       heartbeat interval. if our itimer is off more than 2 seconds
       then we fix it up and reset it */
    struct itimerval itimer;
    getitimer(ITIMER_REAL, &itimer);
    if(itimer.it_value.tv_sec < heartbeat_time.tv_sec - 2) {
        itimer.it_value = heartbeat_time;
        setitimer(ITIMER_REAL, &itimer, NULL);
    }
}

void ws_handle_event(cJSON *event)
{
    int op = cJSON_GetObjectItem(event, "op")->valueint;
    cJSON *data = cJSON_GetObjectItem(event, "d");
    switch(op) {
    case 1: /* Heartbeat request */
        ws_send_heartbeat();
        break;
    case 10: ; /* Hello */
        int heartbeat_wait = cJSON_GetObjectItem(data,
                "heartbeat_interval")->valueint;
        float jitter = (float)rand() / (RAND_MAX * 1.0f);

        heartbeat_time.tv_sec = heartbeat_wait / 1000;
        heartbeat_time.tv_usec = (heartbeat_wait % 1000) * 1000;
        struct timeval jitter_time = {
            .tv_sec = heartbeat_time.tv_sec * jitter,
            .tv_usec = heartbeat_time.tv_usec * jitter,
        };
        struct itimerval new_itimer = {
            .it_interval = heartbeat_time,
            .it_value = jitter_time
        };
        setitimer(ITIMER_REAL, &new_itimer, NULL);
        break;
    case 11: /* Heartbeat ACK */
        break;
    default:
        print(LOG_ERR "ws: received unknown WS opcode %d", op);
        break;
    }
}
