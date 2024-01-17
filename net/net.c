#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <init.h>
#include <log.h>
#include <subsys.h>

extern int http_get(char *url);
extern void ws_handle_event(cJSON *event);
extern void ws_send_heartbeat();
CURL *ws_handle;
char *gateway_url;

int net_subsystem(void)
{
    print(LOG_INFO "net: starting net subsystem");

    /* Set handler for heartbeats */
    /*
    struct sigaction *alrmhandler = malloc(sizeof(struct sigaction));
    memset(alrmhandler, 0, sizeof(struct sigaction));
    alrmhandler->sa_handler = &ws_send_heartbeat;
    alrmhandler->sa_flags |= SA_RESTART;
    sigaction(SIGALRM, alrmhandler, NULL);
    free(alrmhandler);
    */

    if(!gateway_url)
        panic("net: gateway url invalid");

    ws_handle = curl_easy_init();

    curl_easy_setopt(ws_handle, CURLOPT_URL, gateway_url);
    curl_easy_setopt(ws_handle, CURLOPT_CONNECT_ONLY, 2L);

    print(LOG_INFO "net: opening ws");
    CURLcode ret = curl_easy_perform(ws_handle);

    if(ret > 0)
        panic("net: cannot open websocket (curl errno %d)", ret);

    int ws_sockfd;
    if((ret = curl_easy_getinfo(ws_handle,
                    CURLINFO_ACTIVESOCKET, &ws_sockfd)) != CURLE_OK)
        panic("net: curl cannot get active socket (errno %d)", ret);

/*    struct pollfd ws_sockpoll = {
        .fd = ws_sockfd,
        .events = POLLIN
    }; */
    char *inbuf = malloc(1<<16 * sizeof(char));
    size_t rlen;
    const struct curl_ws_frame *meta;

    /* Block ALRM */
    sigset_t *set = malloc(sizeof(sigset_t));
    sigaddset(set, SIGALRM);
    sigprocmask(SIG_BLOCK, set, NULL);
    int alrmfd = signalfd(-1, set, 0);
    free(set);

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

    errno = 0;
    do {
        if((sockpoll->revents & POLLIN) == POLLIN) {
            ret = curl_ws_recv(ws_handle, inbuf, 1<<16, &rlen, &meta);
            if(ret == CURLE_AGAIN)
                continue;
            if(ret != CURLE_OK) {
                print(LOG_ERR "net: encountered curl error while reading socket (curl errno %d)", ret);
                break;
            }

            /* TODO: partial frames */
            if((meta->offset | meta->bytesleft) > 0) {
                print(LOG_ERR "net: dropped partial frame");
                continue;
            }

            cJSON *event = cJSON_ParseWithLength(inbuf, rlen);
            if(!event) {
                print(LOG_ERR "net: dropped malformed frame");
                continue;
            }
            ws_handle_event(event);
            cJSON_Delete(event);
        } else if((sockpoll->revents & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL)) > 0) {
            print(LOG_ERR "net: encountered error on socket (revents %d)", sockpoll->revents);
            break;
        }

        if((alrmpoll->revents & POLLIN) == POLLIN) {
            struct signalfd_siginfo siginfo;
            read(alrmfd, &siginfo, sizeof(struct signalfd_siginfo));
            ws_send_heartbeat();
        }
    } while(poll(pollarray, 2, -1) >= 0);
    if(errno > 0) {
        print(LOG_ERR "net: error encountered while polling (errno %d)", errno);
    }

    free(inbuf);

    curl_easy_cleanup(ws_handle);

    panic("net: websocket closed unexpectedly");

    return 0;
}

void net_get_gateway_url()
{
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

    int fd = http_get("https://discord.com/api/gateway/bot");
    if(fd < 0) {
        print(LOG_ERR "net: failed to get gateway url (error %d)", -fd);
        goto assume;
    }

    char buf[512];
    int buf_length = read(fd, buf, 512);
    close(fd);

    cJSON *gateway_info = cJSON_ParseWithLength(buf, buf_length);
    cJSON *gateway_url_json =
        cJSON_GetObjectItemCaseSensitive(gateway_info, "url");
    if(!cJSON_IsString(gateway_url_json) ||
            gateway_url_json->valuestring == NULL) {
        print(LOG_ERR "net: cannot get gateway url from api "
                "(token invalid?)");
        cJSON_Delete(gateway_info);
        goto assume;
    }

    gateway_url = calloc(strlen(gateway_url_json->valuestring) + 1,
            sizeof(char));
    strcpy(gateway_url, gateway_url_json->valuestring);
    gateway_url[0] = 'W';
    gateway_url[1] = 'S';
    gateway_url[2] = 'S';

    print(LOG_DEBUG "net: using gateway url %s", gateway_url);

    cJSON_Delete(gateway_info);
    return;

assume:
    print(LOG_DEBUG "net: assuming gateway url WSS://gateway.discord.gg");
    gateway_url = calloc(strlen("WSS://gateway.discord.gg") + 1,
            sizeof(char));
    strcpy(gateway_url, "WSS://gateway.discord.gg");
    return;
}
l1_initcall(net_get_gateway_url);

void net_initcall()
{
    start_subsystem(net_subsystem);
}
l2_initcall(net_initcall);
