#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

extern char *token;

int http_get(char *url)
{
    int pipefd[2];
    if(pipe(pipefd) < 0)
        return -errno;

    FILE *write_end = fdopen(pipefd[1], "w");

    int ret = pipefd[0];

    CURL *job = curl_easy_init();
    if(job == 0) {
        close(pipefd[0]);
        ret = -1;
        goto close_writepipe;
    }

    curl_easy_setopt(job, CURLOPT_URL, url);
    curl_easy_setopt(job, CURLOPT_WRITEDATA, write_end);
    curl_easy_setopt(job, CURLOPT_HTTPHEADER, curl_slist_append(NULL, token));
    CURLcode res = curl_easy_perform(job);

    if(res > 0) {
        ret = -res;
        close(pipefd[0]);
        goto cleanup;
    }

cleanup:
    curl_easy_cleanup(job);
close_writepipe:
    fclose(write_end);
    return ret;
}
