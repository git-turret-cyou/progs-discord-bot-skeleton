#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#define __API_INTERNAL
#include <api.h>
#include <log.h>

extern char *token;

int http_request(HTTPMethod method, char *url,
        struct curl_slist *_Nullable headers, char *writebuf, size_t bufsiz)
{
    int inputpipe[2];
    int outputpipe[2];

    if(pipe(inputpipe) < 0)
        return -(errno << 8);
    if(pipe(outputpipe) < 0)
        return -(errno << 8);

    if(writebuf && bufsiz > 0)
        write(inputpipe[1], writebuf, bufsiz);
    close(inputpipe[1]);

    FILE *input_read = fdopen(inputpipe[0], "r");
    FILE *output_write = fdopen(outputpipe[1], "w");

    int ret = outputpipe[0];

    CURL *job = curl_easy_init();
    if(job == NULL)
        panic("api: curl_easy_init failed");

    curl_easy_setopt(job, CURLOPT_URL, url);
    curl_easy_setopt(job, CURLOPT_READDATA, input_read);
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
    if(headers)
        curl_easy_setopt(job, CURLOPT_HTTPHEADER, headers);
    CURLcode res = curl_easy_perform(job);

    if(res > 0) {
        close(outputpipe[0]);
        ret = -res;
    }

    curl_easy_cleanup(job);
    fclose(input_read);
    fclose(output_write);
    return ret;
}

int api_request(HTTPMethod method, char *url,
        struct curl_slist *_Nullable headers, char *writebuf, size_t bufsiz)
{
    char *new_url = calloc((strlen("https://discord.com/api") + strlen(url) + 1),
            sizeof(char));
    strcpy(new_url, "https://discord.com/api");
    strcat(new_url, url);
    struct curl_slist *headers_auth = curl_slist_append(headers, token);
    int ret = http_request(method, new_url, headers_auth, writebuf, bufsiz);
    free(new_url);
    curl_slist_free_all(headers_auth);
    return ret;
}
