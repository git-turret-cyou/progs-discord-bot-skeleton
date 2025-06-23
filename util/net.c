#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <dbs/api.h>
#include <dbs/event.h>
#include <dbs/init.h>
#include <dbs/log.h>
#include <dbs/subsys.h>

/* functions */
int http_request(HTTPMethod method, char *url,
        struct curl_slist *headers, char *writebuf,
        long *response_code);
int api_request(HTTPMethod method, char *url,
        struct curl_slist *headers, char *writebuf,
        long *response_code);
static void setup_token_header();

static int (**ev_get_handler(enum Event event)) (cJSON *);
int ev_set_handler(enum Event event, int (*ev_handler)(cJSON*));

static void ws_send_heartbeat();
static void ws_handle_event(cJSON *event);

int net_subsystem();
void net_get_gateway_url();

/* variables */
static struct {
#define E(_, ev_name) int (*ev_name)(cJSON *);
#include <dbs/bits/events.h>
#undef E
} ev_handlers;

CURL *ws_handle;
static char *gateway_url;
static char *token_header;

static struct timeval last_heartbeat_sent;
double api_latency;

static long last_sequence = -1;
static struct timeval heartbeat_time;

int http_request(HTTPMethod method, char *url,
        struct curl_slist *headers, char *writebuf,
        long *response_code)
{
    /* TODO: async */
    int outputpipe[2];

    if(pipe(outputpipe) < 0)
        return -(errno << 8);

    FILE *output_write = fdopen(outputpipe[1], "w");

    int ret = outputpipe[0];

    CURL *job = curl_easy_init();
    if(job == NULL)
        panic("api: curl_easy_init failed");

    curl_easy_setopt(job, CURLOPT_URL, url);
    curl_easy_setopt(job, CURLOPT_WRITEDATA, output_write);
    char *requestmethod = "GET";
    switch(method) {
    case HTTP_PATCH:
        requestmethod = "PATCH";
        break;
    case HTTP_DELETE:
        requestmethod = "DELETE";
        break;
    case HTTP_PUT:
        requestmethod = "PUT";
        break;
    case HTTP_POST:
        requestmethod = "POST";
        break;
    case HTTP_GET: /* fallthrough */
    default:
        break;
    }
    curl_easy_setopt(job, CURLOPT_CUSTOMREQUEST, requestmethod);
    if(method != HTTP_GET && writebuf != NULL) {
        curl_easy_setopt(job, CURLOPT_POSTFIELDS, writebuf);
    }
    if(headers)
        curl_easy_setopt(job, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(job);

    if(res > 0) {
        close(outputpipe[0]);
        ret = -res;
    }

    if(response_code != NULL) {
        curl_easy_getinfo(job, CURLINFO_RESPONSE_CODE, response_code);
    }

    curl_easy_cleanup(job);
    fclose(output_write);
    return ret;
}

static void setup_token_header()
{
    if(token_header != NULL)
        return;
    char *token = getenv("TOKEN");
    if(!token)
        panic("api: cannot find TOKEN in env");
    token_header = calloc(strlen(token) + strlen("Authorization: Bot ") + 1, sizeof(char));
    strcpy(token_header, "Authorization: Bot ");
    strcat(token_header, token);
}
l1_initcall(setup_token_header);

int api_request(HTTPMethod method, char *url,
        struct curl_slist *headers, char *writebuf,
        long *response_code)
{
    char *new_url = calloc((strlen("https://discord.com/api") + strlen(url) + 1),
            sizeof(char));
    strcpy(new_url, "https://discord.com/api");
    strcat(new_url, url);
    if(token_header == NULL)
        setup_token_header();
    struct curl_slist *headers_auth = curl_slist_append(headers, token_header);
    int ret = http_request(method, new_url, headers_auth, writebuf, response_code);
    free(new_url);
    curl_slist_free_all(headers_auth);
    return ret;
}

/* returns the pointer of the function pointer in the ev_handlers struct */
static int (**ev_get_handler(enum Event event)) (cJSON *)
{
    switch(event) {
#define E(ev_enum, ev_func) \
    case ev_enum: \
        return &(ev_handlers.ev_func);
#include <dbs/bits/events.h>
#undef E
    default:
        return &(ev_handlers.event_invalid);
    }
}

int ev_set_handler(enum Event event, int (*ev_handler)(cJSON*))
{
    int (**ev_pointer)(cJSON*) = ev_get_handler(event);
    *ev_pointer = ev_handler;
    return 0;
}

static void ws_send_heartbeat()
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

    gettimeofday(&last_heartbeat_sent, NULL);
}

static void ws_handle_event(cJSON *event)
{
    int op = cJSON_GetObjectItem(event, "op")->valueint;
    char *msg;
    cJSON *data = cJSON_GetObjectItem(event, "d");
    switch(op) {
    case 0: ; /* Event dispatch */
        cJSON *ev_name = cJSON_GetObjectItem(event, "t");
        if(!cJSON_IsString(ev_name)) {
            print(LOG_ERR "ws: malformed event dispatch (t not a string)");
            break;
        }

        char *event = ev_name->valuestring;
        enum Event ev;
#define E(ev_name, _) \
        if (strcmp(event, #ev_name ) == 0) { \
            ev = ev_name; \
        } else
#include <dbs/bits/events.h>
#undef E
               { ev = EVENT_INVALID; }

        int (*ev_handler)(cJSON *) = *ev_get_handler(ev);
        /* TODO: intercept ready event for session information */
        if(ev_handler != NULL) {
            ev_handler(data);
        } else {
            char *ev_payload = cJSON_Print(data);
            print("ws: unhandled event %s (data below)\n%s", event, ev_payload);
            free(ev_payload);
        }
        break;
    case 1: /* Heartbeat request */
        ws_send_heartbeat();
        break;
    case 9: /* Invalid Session */
        if(!cJSON_IsTrue(data)) {
            /* discord sets data to true if we can reconnect,
               but in this statement it is false, so we just die */
            /* note: discord closes the websocket after sending this,
               so we let our ws code accept and handle the error */
            break;
        }
        /* FALLTHROUGH */
    case 7: ; /* Reconnect */
        /* TODO */
        msg = cJSON_Print(data);
        panic("ws: cannot reconnect to ws after failure (Not supported)\n%s", msg);
        free(msg); /* at least the effort is there (not a memory leak) */
        break;
    case 10: ; /* Hello */
        /* Establish heartbeat interval*/
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

        /* Send IDENT */
        /* TODO: resume functionality */
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

        msg = cJSON_PrintUnformatted(ev_payload);
        size_t sent;
        curl_ws_send(ws_handle, msg, strlen(msg), &sent, 0, CURLWS_TEXT);
        free(msg);
        cJSON_Delete(ev_payload);
        print(LOG_DEBUG "hello: sent IDENT");

        /* Call user hello handler */
        int (*hello_handler)(cJSON *) = *ev_get_handler(HELLO);
        if(hello_handler) {
            (hello_handler)(data);
        }

        break;
    case 11: ; /* Heartbeat ACK */
        // last_heartbeat_sent
        struct timeval now;
        gettimeofday(&now, NULL);
        api_latency = (now.tv_sec - last_heartbeat_sent.tv_sec) * 1000.0f
            + (now.tv_usec - last_heartbeat_sent.tv_usec) / 1000.0f;
        print(LOG_DEBUG "ws: heartbeat ACK (latency %.4f)", api_latency);

        break;
    default:
        print(LOG_ERR "ws: received unknown WS opcode %d", op);
        break;
    }
}

static void ws_handle_close(short code, char *msg) {
    switch(code){
    case 4003: /* Not authenticated */
    case 4007: /* Invalid seq */
    case 4009: /* Session timed out */
        /* Reconnect is allowed, however our previous session has been invalidated,
           so we need to create a new session */
        panic("ws code %d: %s", code, msg);
        break;
        /* TODO: handle new session reconnect */

    case 4004: /* Authentication failed */
    case 4010: /* Invalid shard */
    case 4011: /* Sharding required */
    case 4012: /* Invalid API version */
    case 4013: /* Invalid intent(s) */
    case 4014: /* Disallowed intent(s) */
        /* We should not try to reconnect! These are issues either
           with DBS or with configuration and need to be resolved manually. */
        panic("ws code %d: %s", code, msg);
        break;

    default:
        /* All other codes have us reconnect, reusing our established session so
           data is not lost */
        panic("ws code %d: %s", code, msg);
        /* TODO: handle session reuse reconnect */
        break;
    }
}

int net_subsystem(void)
{
    if(!gateway_url)
        panic("net: gateway url invalid");

    /* Initialise CURL */
    ws_handle = curl_easy_init();

    curl_easy_setopt(ws_handle, CURLOPT_URL, gateway_url);
    curl_easy_setopt(ws_handle, CURLOPT_CONNECT_ONLY, 2L);

    CURLcode ret = curl_easy_perform(ws_handle);

    if(ret > 0) {
        panic("net: cannot open websocket: %s", curl_easy_strerror(ret));
    }

    int ws_sockfd;
    if((ret = curl_easy_getinfo(ws_handle,
                    CURLINFO_ACTIVESOCKET, &ws_sockfd)) != CURLE_OK)
        panic("net: curl cannot get active socket: "
                "%s", curl_easy_strerror(ret));


    /* TODO: catch sigterm & terminate session */
    /* Block ALRM */
    sigset_t *set = malloc(sizeof(sigset_t));
    sigemptyset(set);
    sigaddset(set, SIGALRM);
    sigprocmask(SIG_BLOCK, set, NULL);
    int alrmfd = signalfd(-1, set, 0);
    free(set);

    /* Prepare poll */
    struct pollfd pollarray[2] = {
        {
            .fd = ws_sockfd,
            .events = POLLIN,
            .revents = POLLIN
        },
        {
            .fd = alrmfd,
            .events = POLLIN,
            .revents = 0
        }
    };

    struct pollfd *sockpoll = &(pollarray[0]);
    struct pollfd *alrmpoll = &(pollarray[1]);

    /* Misc. variables */
    char *inbuf = malloc(1<<16 * sizeof(char));
    size_t rlen;
    const struct curl_ws_frame *meta;

    errno = 0;
    do {
        if((sockpoll->revents & POLLIN) == POLLIN) {
            ret = curl_ws_recv(ws_handle, inbuf, 1<<16, &rlen, &meta);
            /* sometimes only SSL information gets sent through, so no actual
               data is received. curl uses NONBLOCK internally so it lets us
               know if there is no more data remaining */
            if(ret == CURLE_AGAIN)
                goto sockpoll_continue;
            if(ret != CURLE_OK) {
                print(LOG_ERR "net: encountered error while reading socket: "
                        "%s", curl_easy_strerror(ret));
                break;
            }

            /* TODO: partial frames */
            if((meta->offset | meta->bytesleft) > 0) {
                print(LOG_ERR "net: dropped partial frame");
                goto sockpoll_continue;
            }

            switch(meta->flags) {
            case(CURLWS_PING):
                curl_ws_send(ws_handle, NULL, 0, NULL, 0, CURLWS_PONG);
                goto sockpoll_continue;
            case(CURLWS_CLOSE):;
                short code = (uint8_t)inbuf[1] | (uint8_t)inbuf[0] << 8;
                char *msg = (char*)((void*)inbuf + 2);
                inbuf[rlen] = '\0';
                ws_handle_close(code, msg);
                goto sockpoll_continue;
            default:
                break;
            }

            cJSON *event = cJSON_ParseWithLength(inbuf, rlen);
            if(!event) {
                fwrite(inbuf, rlen, sizeof(char), stdout);
                print(LOG_ERR "net: dropped malformed frame");
                goto sockpoll_continue;
            }
            ws_handle_event(event);
            cJSON_Delete(event);
        } else if((sockpoll->revents &
                    (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL)) > 0) {
            break;
        }
sockpoll_continue:

        if((alrmpoll->revents & POLLIN) == POLLIN) {
            struct signalfd_siginfo siginfo;
            read(alrmfd, &siginfo, sizeof(struct signalfd_siginfo));
            ws_send_heartbeat();
        }
    } while(poll(pollarray, 2, -1) >= 0);

    if(errno > 0) {
        print(LOG_ERR "net: poll: %s", strerror(errno));
    }

    free(inbuf);

    curl_easy_cleanup(ws_handle);

    panic("net: websocket closed unexpectedly");

    return 0;
} /* net_subsystem */
declare_subsystem(net_subsystem);

void net_get_gateway_url()
{
    /* determine if websockets are supported */
    curl_version_info_data *curl_version =
        curl_version_info(CURLVERSION_NOW);
    const char * const* curl_protocols = curl_version->protocols;
    int wss_supported = 0;
    for(int i = 0; curl_protocols[i]; ++i) {
        if(strcmp(curl_protocols[i], "wss") == 0) {
            wss_supported = 1;
            break;
        }
    }

    if(!wss_supported)
        panic("net: wss not supported by libcurl");

    /* fetch preferred url from discord */
    long response_code;
    int fd = api_get("/gateway/bot", NULL, NULL, &response_code);
    if(fd < 0) {
        print(LOG_ERR "net: cannot get gateway url: %s", curl_easy_strerror(-fd));
        goto assume;
    }

    if(response_code != 200) {
        switch(response_code){
        case 401:
            panic("net: token is invalid, please use a valid token");
            break;
        default:
            panic("net: received non-OK status while asking for url (code %d)", response_code);
            break;
        }
    }

    char buf[512];
    int buf_length = read(fd, buf, 512);
    close(fd);

    cJSON *gateway_info = cJSON_ParseWithLength(buf, buf_length);
    cJSON *gateway_url_json =
        cJSON_GetObjectItemCaseSensitive(gateway_info, "url");
    if(!cJSON_IsString(gateway_url_json) ||
            gateway_url_json->valuestring == NULL) {

        cJSON *gateway_message =
            cJSON_GetObjectItemCaseSensitive(gateway_info, "message");

        if(cJSON_IsString(gateway_message)) {
            print(LOG_ERR "net: cannot get gateway url from api: "
                    "%s: assuming url", cJSON_GetStringValue(gateway_message));
        } else {
            print(LOG_ERR "net: cannot get gateway url from api "
                    "(unknown error): assuming url");
        }
        cJSON_Delete(gateway_info);
        goto assume;
    }

    /* curl requires websocket secure URLs to begin with WSS instead
       of wss, so we fix up the received url for curl */
    gateway_url = calloc(strlen(gateway_url_json->valuestring) + 1,
            sizeof(char));
    strcpy(gateway_url, gateway_url_json->valuestring);
    gateway_url[0] = 'W';
    gateway_url[1] = 'S';
    gateway_url[2] = 'S';

    cJSON_Delete(gateway_info);
    return;

assume:
    gateway_url = calloc(strlen("WSS://gateway.discord.gg") + 1,
            sizeof(char));
    strcpy(gateway_url, "WSS://gateway.discord.gg");
    return;
}
l1_initcall(net_get_gateway_url);
