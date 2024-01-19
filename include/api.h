#ifndef __API_H
#define __API_H
#define _Nullable

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH
} HTTPMethod;

#ifndef __API_INTERNAL

int http_request(HTTPMethod method, char *url,
        struct curl_slist *_Nullable headers, char *writebuf, size_t bufsiz);

int api_request(HTTPMethod method, char * url,
        struct curl_slist *_Nullable headers, char *writebuf, size_t bufsiz);

#endif

#define http_get(...) http_request(HTTP_GET, __VA_ARGS__)
#define http_post(...) http_request(HTTP_POST, __VA_ARGS__)
#define http_put(...) http_request(HTTP_PUT, __VA_ARGS__)
#define http_delete(...) http_request(HTTP_DELETE, __VA_ARGS__)
#define http_patch(...) http_request(HTTP_PATCH, __VA_ARGS__)

#define api_get(...) api_request(HTTP_GET, __VA_ARGS__)
#define api_post(...) api_request(HTTP_POST, __VA_ARGS__)
#define api_put(...) api_request(HTTP_PUT, __VA_ARGS__)
#define api_delete(...) api_request(HTTP_DELETE, __VA_ARGS__)
#define api_patch(...) api_request(HTTP_PATCH, __VA_ARGS__)

#endif
