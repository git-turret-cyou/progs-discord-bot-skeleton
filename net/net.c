#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>
#include <curl/curl.h>

#include <init.h>
#include <log.h>
#include <subsys.h>

extern int http_get(char *url);
char *gateway_url;

int net_subsystem(void)
{
    print(LOG_INFO "net: starting net subsystem");

    return 0;
}

void net_get_gateway_url()
{
    curl_version_info_data *curl_version = curl_version_info(CURLVERSION_NOW);
    const char * const* curl_protocols = curl_version->protocols;
    int wss_supported = 0;
    for(int i = 0; curl_protocols[i]; ++i) {
        if(strcmp(curl_protocols[i], "wss") == 0)
            wss_supported = 1;
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
    cJSON *gateway_url_json = cJSON_GetObjectItemCaseSensitive(gateway_info, "url");
    if(!cJSON_IsString(gateway_url_json) || gateway_url_json->valuestring == NULL) {
        print(LOG_ERR "net: cannot get gateway url from api (token invalid?)");
        cJSON_Delete(gateway_info);
        goto assume;
    }

    char *gateway_url = calloc(strlen(gateway_url_json->valuestring) + 1, sizeof(char));
    strcpy(gateway_url, gateway_url_json->valuestring);
    print(LOG_DEBUG "net: using gateway url %s", gateway_url);

    free(gateway_url);
    cJSON_Delete(gateway_info);
    return;

assume:
    print(LOG_DEBUG "net: assuming gateway url wss://gateway.discord.gg");
    gateway_url = calloc(strlen("wss://gateway.discord.gg") + 1, sizeof(char));
    strcpy(gateway_url, "wss://gateway.discord.gg");
    return;
}
l1_initcall(net_get_gateway_url);

void net_initcall()
{
    start_subsystem(net_subsystem);
}
l2_initcall(net_initcall);
