#ifndef __HTTP__
#define __HTTP__

#include "aostr.h"
#include "list.h"
#include "json.h"

#define RES_TYPE_INVALID (0 << 1)
#define RES_TYPE_HTML    (1 << 1)
#define RES_TYPE_TEXT    (2 << 1)
#define RES_TYPE_JSON    (3 << 1)

#define HTTP_ERR 0
#define HTTP_OK  1

typedef size_t httpStreamCallBack(char *stream, size_t size, size_t nmemb,
                                  void **userdata);

typedef struct httpResponse {
    aoStr *body;
    unsigned int bodylen;
    unsigned int status_code;
    int content_type;
} httpResponse;

void httpResponseRelease(httpResponse *response);
void httpPrintResponse(httpResponse *response);

httpResponse *curlHttpGet(char *url, list *headers, int flags);
httpResponse *curlHttpPost(char *url, list *headers, aoStr *payload, int flags);
json *curlHttpGetJSON(char *url, list *headers, int flags);
json *curlHttpPostJSON(char *url, list *headers, aoStr *payload,
                       int flags);
int curlHttpStreamPost(char *url, list *headers, aoStr *payload,
                       void **privdata, httpStreamCallBack *callback,
                       int flags);

#endif
