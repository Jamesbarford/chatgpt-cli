/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <curl/curl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "aostr.h"
#include "http.h"
#include "json.h"
#include "openai.h"
#include "panic.h"

static httpResponse *httpResponseNew(void) {
    httpResponse *res;

    if ((res = malloc(sizeof(httpResponse))) == NULL)
        return NULL;

    res->body = NULL;
    res->bodylen = 0;
    /* Start in error state */
    res->status_code = 404;

    return res;
}

void httpResponseRelease(httpResponse *response) {
    if (response) {
        aoStrRelease(response->body);
        free(response);
    }
}

static int _httpGetContentType(char *type) {
    if (strncasecmp(type, "application/json", 16) == 0)
        return RES_TYPE_JSON;
    if (strncasecmp(type, "text/html", 9) == 0)
        return RES_TYPE_HTML;
    if (strncasecmp(type, "text", 4) == 0)
        return RES_TYPE_TEXT;

    return RES_TYPE_INVALID;
}

void httpPrintResponse(httpResponse *response) {
    if (response == NULL) {
        printf("(null)\n");
        return;
    }
    char *content_type;
    content_type = "text";

    switch (response->content_type) {
    case RES_TYPE_HTML:
        content_type = "html";
        break;
    case RES_TYPE_INVALID:
        content_type = "invalid";
        break;
    case RES_TYPE_JSON:
        content_type = "json";
        break;
    case RES_TYPE_TEXT:
        content_type = "text";
        break;
    }

    printf("status code: %d\n"
           "body length: %d\n"
           "content type: %s\n"
           "body: %s\n",
           response->status_code, response->bodylen, content_type,
           response->body->data);
}

static size_t httpRequestWriteCallback(char *ptr, size_t size, size_t nmemb,
                                       void **userdata) {
    aoStr **str = (aoStr **)userdata;
    size_t rbytes = size * nmemb;
    aoStrCatLen(*str, ptr, rbytes);
    return rbytes;
}

#define HTTP_REQ_GET  0
#define HTTP_REQ_POST 1

static httpResponse *curlMakeRequest(char *url, int req_type, list *headers,
                                     aoStr *payload, int flags) {
    CURL *curl;
    CURLcode res;
    httpResponse *httpres;
    char *contenttype = NULL;
    aoStr *respbody = aoStrAlloc(512);
    long http_code = 0;

    struct curl_slist *curl_headers = NULL;
    curl_headers = curl_slist_append(curl_headers,
                                     "Content-Type: application/json");
    if (headers) {
        list *node = headers->next;
        while (node != headers) {
            curl_headers = curl_slist_append(curl_headers,
                                             aoStrGetData(node->value));
            node = node->next;
        }
    }

    if ((httpres = httpResponseNew()) == NULL) {
        return NULL;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        if (req_type == HTTP_REQ_POST && payload) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload->data);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpRequestWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respbody);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        if (flags & OPEN_AI_FLAG_VERBOSE) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK) {
            aoStrRelease(respbody);
            warning("Failed to make request: %s\n", curl_easy_strerror(res));
            return NULL;
        } else {
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contenttype);
            httpres->status_code = http_code;
            httpres->content_type = _httpGetContentType(contenttype);
            httpres->body = respbody;
            httpres->bodylen = respbody->len;

            if (httpres->status_code == 200 &&
                httpres->content_type == RES_TYPE_JSON) {
                curl_easy_cleanup(curl);
                curl_slist_free_all(curl_headers);
                return httpres;
            }
        }

        curl_easy_cleanup(curl);
    }

    curl_slist_free_all(curl_headers);
    return NULL;
}

/* This is a bit nuts, it streams data from an endpoint repeaditly calling the
 * `callback` with `privdata`, there is no point in accumulating all of the
 * data and returning it as it is too slow. However the stream is fast */
int curlHttpStreamPost(char *url, list *headers, aoStr *payload,
                       void **privdata, httpStreamCallBack *callback,
                       int flags) {
    CURL *curl;
    CURLcode res;
    long http_code = 0;

    struct curl_slist *curl_headers = NULL;
    curl_headers = curl_slist_append(curl_headers,
                                     "Content-Type: application/json");
    if (headers) {
        list *node = headers->next;
        while (node != headers) {
            curl_headers = curl_slist_append(curl_headers,
                                             aoStrGetData(node->value));
            node = node->next;
        }
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload->data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, privdata);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        if (flags & OPEN_AI_FLAG_VERBOSE) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res != CURLE_OK) {
            warning("Failed to make request: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    curl_slist_free_all(curl_headers);
    return http_code >= 200 && http_code <= 300;
}

httpResponse *curlHttpGet(char *url, list *headers, int flags) {
    return curlMakeRequest(url, HTTP_REQ_GET, headers, NULL, flags);
}

httpResponse *curlHttpPost(char *url, list *headers, aoStr *payload,
                           int flags) {
    return curlMakeRequest(url, HTTP_REQ_POST, headers, payload, flags);
}

json *curlHttpGetJSON(char *url, list *headers, int flags) {
    httpResponse *response = curlHttpGet(url, headers, flags);
    if (response && response->status_code == 200 &&
        response->content_type == RES_TYPE_JSON) {
        json *j = jsonParseWithLen(response->body->data, response->bodylen);
        return j;
    }
    httpResponseRelease(response);
    return NULL;
}

json *curlHttpPostJSON(char *url, list *headers, aoStr *payload, int flags) {
    httpResponse *response = curlHttpPost(url, headers, payload, flags);
    if (response && response->status_code == 200 &&
        response->content_type == RES_TYPE_JSON) {
        json *j = jsonParseWithLen(response->body->data, response->bodylen);
        return j;
    }
    httpResponseRelease(response);
    return NULL;
}
