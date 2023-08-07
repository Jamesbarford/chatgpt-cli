/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aostr.h"
#include "io.h"
#include "json-selector.h"
#include "json.h"
#include "linenoise.h"
#include "openai.h"
#include "panic.h"

char *getApiKey(void) {
    char *apikey = NULL;
    int fd;

    if ((apikey = getenv("OPEN_API_KEY")) != NULL) {
        return apikey;
    }

    if ((fd = open(".env", O_RDONLY, 0666)) == -1) {
        panic("Failed to read .env file and get OPEN_API_KEY\n");
    }

    off_t len = lseek(fd, 0, SEEK_END);
    if (len == 0) {
        panic("Failed to read .env file and get OPEN_API_KEY, file has no contents\n");
    }

    lseek(fd, 0, SEEK_SET);
    if ((apikey = malloc(sizeof(char) * len)) == NULL) {
        panic("OOM\n");
    }

    if (read(fd, apikey, len) != len) {
        panic("Failed to read .env file: %s\n", strerror(errno));
    }

    if (apikey[len - 1] == '\n') {
        apikey[len - 1] = '\0';
    }

    return apikey;
}

char *hints(const char *buf, int *color, int *bold) {
    if (!strcasecmp(buf, "/chat")) {
        *color = 35;
        *bold = 0;
        return "<cmd>";
    } else if (!strcasecmp(buf, "/chat h")) {
        *color = 35;
        *bold = 0;
        return "/chat history";
    } else if (!strcasecmp(buf, "/chat c")) {
        *color = 35;
        *bold = 0;
        return "/chat history delete";
    }
    return NULL;
}

void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == '/') {
        if (!strncmp(buf, "/set", 4)) {
            linenoiseAddCompletion(lc, "/set-temp");
            linenoiseAddCompletion(lc, "/set-model");
            linenoiseAddCompletion(lc, "/set-top_p");
            linenoiseAddCompletion(lc, "/set-presence-pen");
            return;
        } else if (!strncmp(buf, "/mod", 3)) {
            linenoiseAddCompletion(lc, "/models");
            return;
        } else if (!strncmp(buf, "/chat", 5)) {
            linenoiseAddCompletion(lc, "/chat history");
            linenoiseAddCompletion(lc, "/chat persist");
            linenoiseAddCompletion(lc, "/chat list");
            linenoiseAddCompletion(lc, "/chat load");
            linenoiseAddCompletion(lc, "/chat history delete");
            return;
        }

        if (!strncmp(buf, "/sys", 4)) {
            linenoiseAddCompletion(lc, "/system");
            return;
        }

        if (!strncmp(buf, "/set-te", 7)) {
            linenoiseAddCompletion(lc, "/set-temp");
        } else if (!strncmp(buf, "/set-m", 6)) {
            linenoiseAddCompletion(lc, "/set-model");
        } else if (!strncmp(buf, "/set-top", 8)) {
            linenoiseAddCompletion(lc, "/set-top_p");
        } else if (!strncmp(buf, "/set-pre", 8)) {
            linenoiseAddCompletion(lc, "/set-presence-pen");
        } else if (!strncmp(buf, "/list-m", 7)) {
            linenoiseAddCompletion(lc, "/list-models");
        }
    }
}

void commandLineInit(void) {
    linenoiseHistoryLoad("history.txt"); /* Load the history at startup */
    linenoiseSetCompletionCallback(completion);
    linenoiseSetHintsCallback(hints);
}

void commandLineRun(openAiCtx *ctx) {
    char *line, *ptr;
    char tmp1[BUFSIZ], tmp2[BUFSIZ];
    int run = 1, tmp1_len, tmp2_len;
    double num = 0.0;
    json *resp;

    commandLineInit();

    while (run) {
        line = linenoise(">>> ");
        if (line == NULL) {
            break;
        }

        if (line[0] != '\0' && line[0] != '/') {
            linenoiseHistoryAdd(line);           /* Add to the history. */
            linenoiseHistorySave("history.txt"); /* Save the history on disk. */
            openAiChatStream(ctx, line);
        } else if (!strncmp(line, "/historylen", 11)) {
            /* The "/historylen" command will change the history len. */
            int len = atoi(line + 11);
            linenoiseHistorySetMaxLen(len);
        } else if (!strncmp(line, "/set-verbose", 11)) {
            openAiCtxSetFlags(ctx, OPEN_AI_FLAG_VERBOSE);
        } else if (!strncmp(line, "/save", 5)) {
            if (ctx->chat_id == 0) {
                openAiCtxDbNewChat(ctx);
                openAiCtxDbSaveHistory(ctx);
            }
            openAiCtxDbSaveHistory(ctx);
        } else if (!strncmp(line, "/autosave", 9)) {
            if (ctx->chat_id == 0) {
                openAiCtxDbNewChat(ctx);
                openAiCtxDbSaveHistory(ctx);
                openAiCtxSetFlags(ctx, OPEN_AI_FLAG_PERSIST);
            }
            openAiCtxDbSaveHistory(ctx);
        } else if (!strncmp(line, "/models", 7)) {
            resp = openAiListModels(ctx);
            if (resp) {
                json *data = jsonSelect(resp, ".data[*]:a");
                if (data) {
                    for (json *tmp = data; tmp != NULL; tmp = tmp->next) {
                        json *id = jsonSelect(tmp, ".id:s");
                        if (id) {
                            printf("%s\n", id->str);
                        }
                    }
                }
            }
            jsonRelease(resp);
        } else if (!strncmp(line, "/exit", 5)) {
            run = 0;
        } else if (!strncmp(line, "/system", 7)) {
            ptr = line;
            while (!isspace(*ptr) && *ptr != '\0') {
                ptr++;
            }
            ptr++;
            if (*ptr == '\0') {
                goto free_line;
            }
            size_t len = strlen(ptr);
            aoStr *msg = aoStrDupRaw(ptr, len, len);
            aoStr *escaped = aoStrEscapeString(msg);
            openAiChatHistoryAppend(ctx, OPEN_AI_ROLE_SYSTEM, "Geof", escaped);
            printf("\033[0;36m[system]: \033[0m Injected\n");
            aoStrRelease(msg);
            aoStrRelease(escaped);
        } else if (!strncmp(line, "/info", 5)) {
            openAiCtxPrint(ctx);
        } else if (!strncmp(line, "/chatf", 6)) {
            /* READ in entire file */
            aoStr *cmd = aoStrAlloc(2048);
            ptr = line;
            tmp1_len = tmp2_len = 0;
            while (!isspace(*ptr) && *ptr != '\0') {
                ptr++;
            }
            if (*ptr == '\0') {
                warning("Usage: <file> <cmd>\n");
                goto free_line;
            }

            ptr++;
            while (!isspace(*ptr) && *ptr != '\0') {
                tmp1[tmp1_len++] = *ptr;
                ptr++;
            }

            if (*ptr == '\0') {
                warning("Usage: <file> <cmd>\n");
                goto free_line;
            }
            tmp1[tmp1_len] = '\0';
            ptr++;

            while (*ptr != '\0') {
                tmp2[tmp2_len++] = *ptr;
                ptr++;
            }

            tmp2[tmp2_len] = '\0';

            aoStr *contents = ioReadFile(tmp1);
            aoStr *escaped = aoStrEscapeString(contents);
            aoStrCatLen(cmd, tmp2, tmp2_len);
            aoStrCatLen(cmd, ": ", 2);
            aoStrCatLen(cmd, escaped->data, escaped->len);
            openAiChatStream(ctx, cmd->data);
            aoStrRelease(contents);
            aoStrRelease(escaped);
            aoStrRelease(cmd);
        } else if (!strncmp(line, "/chat history clear", 19)) {
            openAiCtxHistoryClear(ctx);
        } else if (!strncmp(line, "/chat persist", 13)) {
            openAiCtxSetFlags(ctx, OPEN_AI_FLAG_PERSIST);
            openAiCtxDbInit(ctx);
            openAiCtxDbNewChat(ctx);
        } else if (!strncmp(line, "/chat list", 10)) {
            int count = 0;
            int *ids = openAiCtxDbGetChatIds(ctx, &count);
            for (int i = 0; i < count; ++i) {
                printf("%d\n", ids[i]);
            }
            free(ids);
        } else if (!strncmp(line, "/chat load", 10)) {
            ptr = line;
            int chat_id = -1;
            while (!isdigit(*ptr) && *ptr != '\0') {
                ptr++;
            }
            if (*ptr == '\0') {
                warning("Usage: /chat load <chat_id>\n");
            } else {
                chat_id = atoi(ptr);
                if (chat_id) {
                    openAiCtxLoadChatHistoryById(ctx, chat_id);
                }
            }
        } else if (!strncmp(line, "/chat history", 13)) {
            printf("messages: %zu\n", ctx->chat_len);
            openAiCtxHistoryPrint(ctx);
        } else if (!strncmp(line, "/set-model", 10)) {
            ptr = line;
            while (*ptr != ' ') {
                ptr++;
            }
            ptr++;
            openAiCtxSetModel(ctx, ptr);
        } else if (!strncmp(line, "/set-temp", 9)) {
            num = strtold(line + 9, NULL);
            openAiCtxSetTemperature(ctx, num);
        } else if (!strncmp(line, "/set-top_p", 10)) {
            num = strtold(line + 10, NULL);
            openAiCtxSetTopP(ctx, num);
        } else if (!strncmp(line, "/set-presence-pen", 17)) {
            num = strtold(line + 17, NULL);
            openAiCtxSetPresencePenalty(ctx, num);
        } else if (line[0] == '/') {
            printf("Unreconized command: %s\n", line);
        }
free_line:
        free(line);
    }
}

int main(void) {
    char *apikey = getApiKey();
    openAiCtx *ctx = openAiCtxNew(apikey, "gpt-3.5-turbo", NULL);

    openAiCtxSetFlags(ctx, (OPEN_AI_FLAG_HISTORY));
    openAiCtxDbInit(ctx);
    commandLineRun(ctx);
}
