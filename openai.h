/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __OPENAPI_H
#define __OPENAPI_H

#include <stddef.h>

#include "aostr.h"
#include "json.h"
#include "list.h"
#include "sql.h"

#define OPEN_AI_FLAG_VERBOSE (1)
#define OPEN_AI_FLAG_HISTORY (2)
#define OPEN_AI_FLAG_PERSIST (4)
#define OPEN_AI_FLAG_STREAM  (8)

#define OPEN_AI_ROLE_USER      (0)
#define OPEN_AI_ROLE_ASSISTANT (1)
#define OPEN_AI_ROLE_SYSTEM    (2)
#define OPEN_AI_ROLE_FUNCTION  (4)

typedef struct openAiMessage {
    int role;       /* Required - The role one of system, user, assistant or
                         function */
    aoStr *content; /* Required - contents of the message */
    char *name;     /* Optional - Author of the message */
} openAiMessage;

typedef struct openAiCtx {
    int chat_id;        /* id of the current chat */
    char *apikey;       /* OPEN_API_KEY*/
    char *organisation; /* Organisation, specify which organisation the api key
                           is for */
    char *model;        /* gpt-4 , gpt-3.5-turbo etc... */
    int n; /* How many chat completion choices to generate for each input
              message. */

    list *auth_headers;     /* Needed on every request */
    float presence_penalty; /* Number between -2.0 and 2.0. Positive values
                               penalize new tokens based on whether they appear
                               in the text so far, increasing the model's
                               likelihood to talk about new topics.*/

    size_t max_tokens; /* The maximum number of tokens to generate in the chat
                          completion. */
    float temperature; /* What sampling temperature to use, between 0 and 2.
                          Higher values like 0.8 will make the output more
                          random, while lower values like 0.2 will make it more
                          focused and deterministic. We generally recommend
                          altering this or top_p but not both. */

    float top_p; /* An alternative to sampling with temperature, called nucleus
                    sampling, where the model considers the results of the
                    tokens with top_p probability mass. So 0.1 means only the
                    tokens comprising the top 10% probability mass are
                    considered. */
    int flags;   /* Whether to use the stream api, history etc.. */

    sqlCtx *db; /* Only exists if OPEN_AI_FLAG_PERSIST has been set */

    openAiMessage *chat_history;
    size_t chat_capacity;
    size_t chat_len;
    aoStr *tmp_buffer;
} openAiCtx;

openAiCtx *openAiCtxNew(char *apikey, char *model, char *organisation);
void openAiCtxRelease(openAiCtx *ctx);
void openAiCtxPrint(openAiCtx *ctx);

void openAiCtxSetApiKey(openAiCtx *ctx, char *apikey);
void openAiCtxSetOrganisation(openAiCtx *ctx, char *organisation);
void openAiCtxSetModel(openAiCtx *ctx, char *model);
void openAiCtxSetN(openAiCtx *ctx, int n);
void openAiCtxSetPresencePenalty(openAiCtx *ctx, float presence_penalty);
void openAiCtxSetMaxTokens(openAiCtx *ctx, size_t max_tokens);
void openAiCtxSetTemperature(openAiCtx *ctx, float temperature);
void openAiCtxSetTopP(openAiCtx *ctx, float top_p);
void openAiCtxSetChatHistory(openAiCtx *ctx, openAiMessage *chat_history);
void openAiCtxSetChatLen(openAiCtx *ctx, size_t history_len);
void openAiCtxSetFlags(openAiCtx *ctx, int flags);
void openAiCtxHistoryPrint(openAiCtx *ctx);
void openAiCtxHistoryClear(openAiCtx *ctx);

void openAiChatHistoryAppend(openAiCtx *ctx, int role, char *name, aoStr *data);
json *openAiListModels(openAiCtx *ctx);
json *openAiChat(openAiCtx *ctx, char *msg);
void openAiChatStream(openAiCtx *ctx, char *msg);

/* Database commands */
void openAiCtxDbInit(openAiCtx *ctx);
void openAiCtxDbNewChat(openAiCtx *ctx);
void openAiCtxDbRenameChat(openAiCtx *ctx, int id, char *name);
void openAiCtxDbDeleteChatById(openAiCtx *ctx, int id);
void openAiCtxDbDeleteMessageById(openAiCtx *ctx, int id);
void openAiCtxDbInsertMessage(openAiCtx *ctx, int role, aoStr *msg);
void openAiCtxLoadChatHistoryById(openAiCtx *ctx, int chat_id);
void openAiCtxDbSaveHistory(openAiCtx *ctx);
int *openAiCtxDbGetChatIds(openAiCtx *ctx, int *count);

#endif // !__OPENAPI_H
