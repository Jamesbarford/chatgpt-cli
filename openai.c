/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aostr.h"
#include "http.h"
#include "json-selector.h"
#include "json.h"
#include "list.h"
#include "openai.h"
#include "panic.h"
#include "sql.h"

char *role_to_str[] = {
        [OPEN_AI_ROLE_USER] = "user",
        [OPEN_AI_ROLE_ASSISTANT] = "assistant",
        [OPEN_AI_ROLE_SYSTEM] = "system",
        [OPEN_AI_ROLE_FUNCTION] = "function",
};

list *openAiAuthHeaders(openAiCtx *ctx) {
    list *headers = listNew();
    aoStr *auth = aoStrAlloc(512);
    aoStrCatPrintf(auth, "Authorization: Bearer %s", ctx->apikey);
    listAppend(headers, auth);
    if (ctx->organisation) {
        aoStr *org = aoStrAlloc(512);
        aoStrCatPrintf(org, "OpenAI-Organization: %s", ctx->organisation);
        listAppend(headers, org);
    }
    return headers;
}

static openAiMessage *openAiMessagesNew(int count) {
    openAiMessage *msgs = (openAiMessage *)calloc(count, sizeof(openAiMessage));
    if (msgs == NULL) {
        return NULL;
    }
    return msgs;
}

openAiCtx *openAiCtxNew(char *apikey, char *model, char *organisation) {
    openAiCtx *ctx = (openAiCtx *)malloc(sizeof(openAiCtx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->chat_id = 0;
    ctx->apikey = strdup(apikey);
    ctx->organisation = organisation ? strdup(organisation) : NULL;
    ctx->model = strdup(model);

    ctx->auth_headers = openAiAuthHeaders(ctx);

    /* History */
    ctx->chat_capacity = 10;
    ctx->chat_len = 0;
    ctx->chat_history = openAiMessagesNew(ctx->chat_capacity);

    ctx->db = NULL;

    ctx->top_p = 0;
    ctx->n = 0;
    ctx->temperature = 0;
    ctx->presence_penalty = 0;
    ctx->max_tokens = 0;
    ctx->flags = 0;
    ctx->tmp_buffer = aoStrAlloc(512);
    return ctx;
}

void openAiCtxPrint(openAiCtx *ctx) {
    printf("CTX OPTIONS");
    printf("  organisation: %s\n", ctx->organisation);
    printf("  model: %s\n", ctx->model);
    printf("  n: %d\n", ctx->n);
    printf("  presence_penalty: %f\n", ctx->presence_penalty);
    printf("  max_tokens: %zu\n", ctx->max_tokens);
    printf("  temperature: %f\n", ctx->temperature);
    printf("  top_p: %f\n", ctx->top_p);
    printf("  flags: 0x%X\n", ctx->flags);
}

void openAiCtxHistoryPrint(openAiCtx *ctx) {
    for (size_t i = 0; i < ctx->chat_len; ++i) {
        openAiMessage *msg = &ctx->chat_history[i];
        switch (msg->role) {
        case OPEN_AI_ROLE_USER:
            printf("[user]: %s\n", msg->content->data);
            break;
        case OPEN_AI_ROLE_ASSISTANT:
            printf("\033[0;32m[assistant]:\033[0m %s\n", msg->content->data);
            break;
        case OPEN_AI_ROLE_SYSTEM:
            printf("\033[0;36m[system]:\033[0m %s\n", msg->content->data);
            break;
        case OPEN_AI_ROLE_FUNCTION:
            printf("[function]: %s\n", msg->content->data);
            break;
        }
    }
}

void openAiCtxHistoryClear(openAiCtx *ctx) {
    if (ctx->chat_len == 0) {
        return;
    }
    for (size_t i = 0; i < ctx->chat_len; ++i) {
        openAiMessage *msg = &ctx->chat_history[i];
        aoStrRelease(msg->content);
    }
    free(ctx->chat_history);
    ctx->chat_len = 0;
    ctx->chat_capacity = 10;
    ctx->chat_history = (openAiMessage *)malloc(sizeof(openAiMessage) *
                                                ctx->chat_capacity);
}

void openAiChatHistoryAppend(openAiCtx *ctx, int role, char *name,
                             aoStr *data) {
    openAiMessage *msgs = ctx->chat_history;
    if (ctx->chat_len + 1 >= ctx->chat_capacity) {
        msgs = realloc(msgs, sizeof(openAiMessage) * ctx->chat_capacity * 2);
        if (msgs != NULL) {
            ctx->chat_capacity *= 2;
            ctx->chat_history = msgs;
        } else {
            warning("Possible OOM: Cannot add more history to chat: %s\n",
                    strerror(errno));
        }
    }
    openAiMessage *msg = &msgs[ctx->chat_len++];
    msg->name = name;
    msg->role = role;
    msg->content = data;
}

void openAiCtxRelease(openAiCtx *ctx) {
    if (ctx) {
        return;
    }
    free(ctx->apikey);
    if (ctx->organisation) {
        free(ctx->organisation);
    }
    free(ctx->model);
    free(ctx->chat_history);
    listRelease(ctx->auth_headers, (void (*)(void *))aoStrRelease);
    free(ctx);
}

void openAiCtxSetOrganisation(openAiCtx *ctx, char *organisation) {
    if (ctx->organisation) {
        free(ctx->organisation);
    }
    ctx->organisation = strdup(organisation);
}

void openAiCtxSetModel(openAiCtx *ctx, char *model) {
    if (ctx->model) {
        free(ctx->model);
    }
    ctx->model = strdup(model);
}

void openAiCtxSetN(openAiCtx *ctx, int n) {
    ctx->n = n;
}

void openAiCtxSetPresencePenalty(openAiCtx *ctx, float presence_penalty) {
    ctx->presence_penalty = presence_penalty;
}

void openAiCtxSetMaxTokens(openAiCtx *ctx, size_t max_tokens) {
    ctx->max_tokens = max_tokens;
}

void openAiCtxSetTemperature(openAiCtx *ctx, float temperature) {
    ctx->temperature = temperature;
}

void openAiCtxSetTopP(openAiCtx *ctx, float top_p) {
    ctx->top_p = top_p;
}

void openAiCtxSetChatHistory(openAiCtx *ctx, openAiMessage *chat_history) {
    ctx->chat_history = chat_history;
}

void openAiCtxSetChatLen(openAiCtx *ctx, size_t history_len) {
    ctx->chat_len = history_len;
}

void openAiCtxSetFlags(openAiCtx *ctx, int flags) {
    ctx->flags |= flags;
}

static void openAiAppendOptionsToPayload(openAiCtx *ctx, aoStr *payload,
                                         char *user_msg) {
    aoStrCatPrintf(payload, "{\"model\": \"%s\"", ctx->model);
    if (ctx->n) {
        aoStrCatPrintf(payload, ",\"n\": %d", ctx->n);
    }
    if (ctx->max_tokens) {
        aoStrCatPrintf(payload, ",\"max_tokens\": %d", ctx->max_tokens);
    }
    if (ctx->presence_penalty) {
        aoStrCatPrintf(payload, ",\"presence_penalty\": %1.5f",
                       ctx->presence_penalty);
    }
    if (ctx->temperature) {
        aoStrCatPrintf(payload, ",\"temperature\": %1.5f", ctx->temperature);
    }
    if (ctx->top_p) {
        aoStrCatPrintf(payload, ",\"top_p\": %1.5f", ctx->top_p);
    }
    if (ctx->flags & OPEN_AI_FLAG_HISTORY) {
        /* This can be optimised, by maintaining a string */
        aoStrCatLen(payload, ",\"messages\": [", 14);
        for (size_t i = 0; i < ctx->chat_len; ++i) {
            openAiMessage *msg = &ctx->chat_history[i];
            aoStrCatPrintf(payload, "{\"role\": \"%s\", \"content\": \"%s\"},",
                           role_to_str[msg->role], aoStrGetData(msg->content));
        }
    }
    aoStrCatPrintf(payload, "{\"role\": \"%s\", \"content\": \"%s\"}]",
                   role_to_str[OPEN_AI_ROLE_USER], user_msg);
}

void openAiCtxDbInit(openAiCtx *ctx) {
    if (ctx->db == NULL) {
        ctx->db = sqlCtxNew(SQL_DB_NAME);
        char *sql =
                "CREATE TABLE IF NOT EXISTS chat(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT,"
                "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "model TEXT);\n"
                "CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "chat_id INT,"
                "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "role INT,"
                "msg TEXT,"
                "CONSTRAINT chat_k FOREIGN KEY(chat_id) REFERENCES chat(id) ON DELETE CASCADE);\n";
        char *err = sqlExecRaw(ctx->db, sql);
        if (err) {
            panic("DB initialization error: %s\n", err);
        }
    }
}

void openAiCtxDbSaveHistory(openAiCtx *ctx) {
    /* I do know how to concatinate a string */
    for (size_t i = 0; i < ctx->chat_len; ++i) {
        openAiMessage *msg = &ctx->chat_history[i];
        openAiCtxDbInsertMessage(ctx, msg->role, msg->content);
    }
}

void openAiCtxDbNewChat(openAiCtx *ctx) {
    sqlRow row;
    sqlParam params[1] = {
            {.type = SQL_TEXT, .str = ctx->model},
    };
    sqlQuery(ctx->db, "INSERT INTO chat(model) VALUES(?);", params, 1);
    sqlSelect(ctx->db, &row, "SELECT id FROM chat ORDER BY created LIMIT 1;",
              NULL, 0);
    int id = -1;
    while (sqlIter(&row)) {
        id = row.col[0].integer;
    }
    if (id) {
        ctx->chat_id = id;
    }
}

void openAiCtxDbRenameChat(openAiCtx *ctx, int id, char *name) {
    sqlParam params[2] = {
            {.type = SQL_INT, .integer = id},
            {.type = SQL_TEXT, .str = name},
    };
    sqlQuery(ctx->db, "UPDATE chat SET name = ? WHERE id = ?;", params, 2);
}

void openAiCtxDbDeleteChatById(openAiCtx *ctx, int id) {
    sqlParam params[1] = {
            {.type = SQL_INT, .integer = id},
    };
    sqlQuery(ctx->db, "DELETE FROM chat WHERE id = ?", params, 1);
}

void openAiCtxDbDeleteChatByName(openAiCtx *ctx, char *name) {
    sqlParam params[1] = {
            {.type = SQL_TEXT, .str = name},
    };
    sqlQuery(ctx->db, "DELETE FROM chat WHERE name = ?", params, 1);
}

void openAiCtxDbDeleteMessageById(openAiCtx *ctx, int id) {
    sqlParam params[1] = {
            {.type = SQL_INT, .integer = id},
    };
    sqlQuery(ctx->db, "DELETE FROM messags WHERE id = ?", params, 1);
}

void openAiCtxDbInsertMessage(openAiCtx *ctx, int role, aoStr *msg) {
    sqlParam params[3] = {
            {.type = SQL_INT, .integer = ctx->chat_id},
            {.type = SQL_INT, .integer = role},
            {.type = SQL_TEXT, .str = aoStrGetData(msg)},
    };
    sqlQuery(ctx->db,
             "INSERT INTO messages (chat_id, role, msg) VALUES (?, ?, ?);",
             params, 3);
}

openAiMessage *openAiDbGetMessagesByChatId(openAiCtx *ctx, int chat_id,
                                           int *count) {
    sqlRow row;
    int i = 0, cap = 10;
    openAiMessage *msgs = NULL, *msg;
    sqlParam params[1] = {
            {.type = SQL_INT, .integer = chat_id},
    };

    if (!sqlSelect(
                ctx->db, &row,
                "SELECT messages.role, messages.msg FROM messages WHERE messages.chat_id = ?;",
                params, 1)) {
        return NULL;
    }

    msgs = (openAiMessage *)malloc(sizeof(openAiMessage) * cap);

    while (sqlIter(&row)) {
        if (i + 1 >= cap) {
            msgs = realloc(msgs, sizeof(openAiMessage) * cap * 2);
            cap *= 2;
        }
        msg = &msgs[i++];
        msg->role = row.col[0].integer;
        msg->content = aoStrDupRaw(row.col[1].str, row.col[1].len,
                                   row.col[1].len);
    }
    *count = i;

    return msgs;
}

void openAiCtxLoadChatHistoryById(openAiCtx *ctx, int chat_id) {
    int count = 0;
    openAiMessage *msgs = openAiDbGetMessagesByChatId(ctx, chat_id, &count);
    if (!msgs) {
        return;
    }
    if (ctx->chat_history != NULL) {
        free(ctx->chat_history);
    }
    ctx->chat_id = chat_id;
    ctx->chat_len = count;
    ctx->chat_capacity = count;
    ctx->chat_history = msgs;
}

int *openAiCtxDbGetChatIds(openAiCtx *ctx, int *count) {
    int *arr = NULL;
    sqlRow row;
    int i = 0;
    *count = 0;

    sqlSelect(ctx->db, &row, "SELECT id from chat;", NULL, 0);

    arr = (int *)malloc(sizeof(int) * row.cols);

    while (sqlIter(&row)) {
        arr[i++] = row.col[0].integer;
    }
    *count = i;
    return arr;
}

/*
    {
      "id": "text-search-babbage-doc-001",
      "object": "model",
      "created": 1651172509,
      "owned_by": "openai-dev",
      "permission": [
        {
          "id": "modelperm-dvJNsLdOcnLbIYlRZRnfQAfX",
          "object": "model_permission",
          "created": 1690864628,
          "allow_create_engine": false,
          "allow_sampling": true,
          "allow_logprobs": true,
          "allow_search_indices": true,
          "allow_view": true,
          "allow_fine_tuning": false,
          "organization": "*",
          "group": null,
          "is_blocking": false
        }
      ],
      "root": "text-search-babbage-doc-001",
      "parent": null
    },
*/
json *openAiListModels(openAiCtx *ctx) {
    return curlHttpGetJSON("https://api.openai.com/v1/models",
                           ctx->auth_headers, ctx->flags);
}

static size_t openAiChatStreamCallback(char *stream, size_t size, size_t nmemb,
                                       void **userdata) {
    openAiCtx **ctx = (openAiCtx **)userdata;
    json *j, *sel, *choices = NULL;
    char *ptr = stream;
    size_t rbytes = size * nmemb;

    if ((*ctx)->flags & OPEN_AI_FLAG_VERBOSE) {
        printf("%s\n", stream);
    }

    if (*stream == '{') {
        j = jsonParseWithLen(stream, rbytes);
        if ((sel = jsonSelect(j, ".error.message")) != NULL) {
            prompt_warning("%s\n", sel->str);
            aoStrCat((*ctx)->tmp_buffer, sel->str);
            return rbytes;
        }
    }

    while (*ptr) {
        if (!strncmp(ptr, "data: ", 6)) {
            ptr += 5;
            while (*ptr != '{') {
                ptr++;
            }
            j = jsonParseWithLen(ptr, rbytes - (stream - ptr));
            if (!j) {
                warning("Failed to Parse JSON\n");
            } else if (j && j->state) {
                warning("Failed to Parse JSON");
                jsonPrintError(j);
            }

            choices = jsonSelect(j, ".choices[0]:o");
            sel = jsonSelect(choices, ".delta.content:s");

            if (sel) {
                printf("%s", sel->str);
                fflush(stdout);
                aoStrCat((*ctx)->tmp_buffer, sel->str);
                jsonRelease(j);
            } else {
                sel = jsonSelect(choices, ".finish_reason");
                if (sel) {
                    jsonRelease(j);
                    break;
                }
                sel = jsonSelect(j, ".finish_reason");
                if (sel) {
                    jsonRelease(j);
                    break;
                }

                jsonPrint(j);
            }
        }
        ptr++;
    }

    /* this should be collecting ONLY the message from the json above */
    return rbytes;
}

void openAiChatStream(openAiCtx *ctx, char *msg) {
    aoStr *payload = aoStrAlloc(512);
    size_t msg_len = strlen(msg);
    /* msg gets freed by the caller */
    aoStr *ref = aoStrFromString(msg, msg_len);
    aoStr *user_escaped_msg = NULL, *assistant_escaped_msg = NULL;
    int http_ok = 0;

    user_escaped_msg = aoStrEscapeString(ref);
    free(ref);

    openAiAppendOptionsToPayload(ctx, payload, user_escaped_msg->data);
    aoStrCat(payload, ",\"stream\": true}");

    if (ctx->flags & OPEN_AI_FLAG_VERBOSE) {
        printf("%s\n", aoStrGetData(payload));
    }

    printf("\033[0;32m[%s]:\033[0m ", ctx->model);
    fflush(stdout);
    http_ok = curlHttpStreamPost("https://api.openai.com/v1/chat/completions",
                                 ctx->auth_headers, payload, (void **)&ctx,
                                 openAiChatStreamCallback, ctx->flags);
    if (!http_ok) {
        warning("Failed to make request\n");
        return;
    }
    printf("\n\n");
    assistant_escaped_msg = aoStrEscapeString(ctx->tmp_buffer);

    /* Store in history */
    if (ctx->flags & OPEN_AI_FLAG_HISTORY) {
        openAiChatHistoryAppend(ctx, OPEN_AI_ROLE_USER, NULL, user_escaped_msg);
        openAiChatHistoryAppend(ctx, OPEN_AI_ROLE_ASSISTANT, NULL,
                                assistant_escaped_msg);
    }

    /* Store in db */
    if (ctx->flags & OPEN_AI_FLAG_PERSIST) {
        openAiCtxDbInsertMessage(ctx, OPEN_AI_ROLE_USER, user_escaped_msg);
        openAiCtxDbInsertMessage(ctx, OPEN_AI_ROLE_ASSISTANT,
                                 assistant_escaped_msg);
    }
    aoStrSetLen(ctx->tmp_buffer, 0);
}

/**
 * {
  "id": "chatcmpl-123",
  "object": "chat.completion",
  "created": 1677652288,
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "\n\nHello there, how may I assist you today?",
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 9,
    "completion_tokens": 12,
    "total_tokens": 21
  }
}
 */
json *openAiChat(openAiCtx *ctx, char *msg) {
    aoStr *payload = aoStrAlloc(512);
    openAiAppendOptionsToPayload(ctx, payload, msg);
    aoStrPutChar(payload, '}');
    json *resp = curlHttpPostJSON("https://api.openai.com/v1/chat/completions",
                                  ctx->auth_headers, payload, ctx->flags);
    aoStrRelease(payload);
    return resp;
}
