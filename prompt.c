#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aostr.h"
#include "io.h"
#include "json-selector.h"
#include "json.h"
#include "linenoise.h"
#include "openai.h"
#include "panic.h"

static char *promptHints(const char *buf, int *color, int *bold) {
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

static void promptCompletion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == '/') {
        if (!strncmp(buf, "/set", 4)) {
            linenoiseAddCompletion(lc, "/set temp");
            linenoiseAddCompletion(lc, "/set model");
            linenoiseAddCompletion(lc, "/set top_p");
            linenoiseAddCompletion(lc, "/set presence-pen");
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

        if (!strncmp(buf, "/set te", 7)) {
            linenoiseAddCompletion(lc, "/set temp");
        } else if (!strncmp(buf, "/set m", 6)) {
            linenoiseAddCompletion(lc, "/set-model");
        } else if (!strncmp(buf, "/set top", 8)) {
            linenoiseAddCompletion(lc, "/set-top_p");
        } else if (!strncmp(buf, "/set pre", 8)) {
            linenoiseAddCompletion(lc, "/set-presence-pen");
        } else if (!strncmp(buf, "/list-m", 7)) {
            linenoiseAddCompletion(lc, "/list-models");
        }
    }
}

static void commandChat(openAiCtx *ctx, char *line) {
    ssize_t len = 0;
    json *resp, *sel;

    if (ctx->flags & OPEN_AI_FLAG_STREAM) {
        openAiChatStream(ctx, line);
    } else {
        resp = openAiChat(ctx, line);
        sel = jsonSelect(resp, ".choices[0].message.content:s");

        if (sel) {
            len = strlen(sel->str);
            openAiChatHistoryAppend(ctx, OPEN_AI_ROLE_ASSISTANT, NULL,
                                    aoStrDupRaw(sel->str, len, len));
        }
        jsonRelease(resp);
    }
}

static void commandSave(openAiCtx *ctx, char *line) {
    (void)line;
    if (ctx->chat_id == 0) {
        openAiCtxDbNewChat(ctx);
        openAiCtxDbSaveHistory(ctx);
    }
    openAiCtxDbSaveHistory(ctx);
}

static void commandAutoSave(openAiCtx *ctx, char *line) {
    commandSave(ctx, line);
    openAiCtxSetFlags(ctx, OPEN_AI_FLAG_PERSIST);
}

static void commandModels(openAiCtx *ctx, char *line) {
    (void)line;
    json *resp = openAiListModels(ctx);
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
}

static void commandSystem(openAiCtx *ctx, char *line) {
    char *ptr = line + 7; /* skip /system*/
    if (*(ptr + 1) == '\0') {
        prompt_warning("Usage: /system <cmd>");
        return;
    }
    ssize_t len = strlen(ptr);
    aoStr *msg = aoStrDupRaw(ptr, len, len);
    aoStr *escaped = aoStrEscapeString(msg);
    /* the history now owns the memory of escaped */
    openAiChatHistoryAppend(ctx, OPEN_AI_ROLE_SYSTEM, "Geof", escaped);
    printf("\033[0;36m[system]: \033[0m Injected\n");
    aoStrRelease(msg);
}

static void commandChatF(openAiCtx *ctx, char *line) {
    aoStr *cmdbuffer = aoStrAlloc(2048);
    char *ptr = line + 6; /* skip /chatf */
    char path[BUFSIZ], cmd[BUFSIZ];
    ssize_t pathlen = 0, cmdlen = 0;
    aoStr *file_contents;

    if (*ptr == '\0' || !isspace(*ptr)) {
        prompt_warning("Usage: /chatf <file> <cmd>\n");
        return;
    }
    ptr++;

    while (!isspace(*ptr) && *ptr != '\0') {
        path[pathlen++] = *ptr++;
    }
    if (!isspace(*ptr)) {
        prompt_warning("Usage: /chatf <file> <cmd>\n");
        return;
    }
    ptr++;
    path[pathlen] = '\0';

    while (*ptr != '\0') {
        cmd[cmdlen++] = *ptr++;
    }
    cmd[cmdlen] = '\0';

    if ((file_contents = ioReadFile(path)) == NULL) {
        prompt_warning("Could not read file\n");
        return;
    }

    aoStrCatPrintf(cmdbuffer, "%s : \n ```\n%s\n```", cmd, file_contents->data);
    openAiChatStream(ctx, cmdbuffer->data);
    aoStrRelease(file_contents);
    aoStrRelease(cmdbuffer);
}

static void commandChatHistoryClear(openAiCtx *ctx, char *line) {
    (void)line;
    openAiCtxHistoryClear(ctx);
}

static void commandPersist(openAiCtx *ctx, char *line) {
    (void)line;
    openAiCtxSetFlags(ctx, OPEN_AI_FLAG_PERSIST);
    openAiCtxDbInit(ctx);
    openAiCtxDbNewChat(ctx);
}

static void commandChatList(openAiCtx *ctx, char *line) {
    (void)line;
    int count = 0;
    int *ids = openAiCtxDbGetChatIds(ctx, &count);
    for (int i = 0; i < count; ++i) {
        printf("%d\n", ids[i]);
    }
    free(ids);
}

static void commandChatRename(openAiCtx *ctx, char *line) {
    char newname[BUFSIZ];
    ssize_t newlen = 0;
    char *ptr;

    if (!(ctx->flags & OPEN_AI_FLAG_PERSIST)) {
        warning("Please turn on OPEN_AI_FLAG_PERSIST to create a table entry and rename the chat\n");
        return;
    }
    ptr = line + 12;
    if (!isspace(*ptr)) {
        warning("Usage: /chat rename <name_of_chat>\n");
        return;
    }
    ptr++;;
    while (*ptr != '\0') {
        newname[newlen++] = *ptr++;
    }
    newname[newlen] = '\0';
    openAiCtxDbRenameChat(ctx, ctx->chat_id, newname);
}

/* Can load a chat from the database without saving to that chat */
static void commandChatLoad(openAiCtx *ctx, char *line) {
    char *ptr = line + 10;
    int chat_id = -1;

    if (!isspace(*ptr)) {
        prompt_warning("Usage: /chat load <chat_id>\n");
        return;
    }

    ptr++;

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
}

static void commandChatHistory(openAiCtx *ctx, char *line) {
    (void)line;
    printf("messages: %zu\n", ctx->chat_len);
    openAiCtxHistoryPrint(ctx);
}

static void commandSetModel(openAiCtx *ctx, char *line) {
    char *ptr = line + 10;
    char model[BUFSIZ];
    ssize_t len = 0;
    if (!isspace(*ptr)) {
        prompt_warning("Usage: /set model <model_name>\n");
        return;
    }
    ptr++;
    while (*ptr != '\0') {
        model[len++] = *ptr++;
    }
    model[len] = '\0';
    openAiCtxSetModel(ctx, model);
}

static void promptInit(void) {
    linenoiseHistoryLoad("history.txt"); /* Load the history at startup */
    linenoiseSetCompletionCallback(promptCompletion);
    linenoiseSetHintsCallback(promptHints);
    linenoiseSetMultiLine(1);
}

void promptMain(openAiCtx *ctx) {
    char *line;
    int run = 1;
    double num = 0.0;
    promptInit();

    while (run) {
        line = linenoise(">>> ");
        if (line == NULL) {
            break;
        }

        if (line[0] != '\0' && line[0] != '/') {
            linenoiseHistoryAdd(line);           /* Add to the history. */
            linenoiseHistorySave("history.txt"); /* Save the history on disk. */
            commandChat(ctx, line);
        } else if (!strncmp(line, "/save", 5)) {
            commandSave(ctx, line);
        } else if (!strncmp(line, "/autosave", 9)) {
            commandAutoSave(ctx, line);
        } else if (!strncmp(line, "/models", 7)) {
            commandModels(ctx, line);
        } else if (!strncmp(line, "/exit", 5)) {
            run = 0;
        } else if (!strncmp(line, "/system", 7)) {
            commandSystem(ctx, line);
        } else if (!strncmp(line, "/info", 5)) {
            openAiCtxPrint(ctx);
        } else if (!strncmp(line, "/chatf", 6)) {
            commandChatF(ctx, line);
        } else if (!strncmp(line, "/chat history clear", 19)) {
            commandChatHistoryClear(ctx, line);
        } else if (!strncmp(line, "/chat history", 13)) {
            commandChatHistory(ctx, line);
        } else if (!strncmp(line, "/chat persist", 13)) {
            commandPersist(ctx, line);
        } else if (!strncmp(line, "/chat list", 10)) {
            commandChatList(ctx, line);
        } else if (!strncmp(line, "/chat rename", 12)) {
            commandChatRename(ctx, line);
        } else if (!strncmp(line, "/chat load", 10)) {
            commandChatLoad(ctx, line);
        } else if (!strncmp(line, "/set model", 10)) {
            commandSetModel(ctx, line);
        } else if (!strncmp(line, "/set verbose", 11)) {
            openAiCtxSetFlags(ctx, OPEN_AI_FLAG_VERBOSE);
        } else if (!strncmp(line, "/set temp", 9)) {
            num = strtold(line + 9, NULL);
            openAiCtxSetTemperature(ctx, num);
        } else if (!strncmp(line, "/set top_p", 10)) {
            num = strtold(line + 10, NULL);
            openAiCtxSetTopP(ctx, num);
        } else if (!strncmp(line, "/set presence-pen", 17)) {
            num = strtold(line + 17, NULL);
            openAiCtxSetPresencePenalty(ctx, num);
        } else if (line[0] == '/') {
            printf("Unreconized command: %s\n", line);
        }
        free(line);
    }
}
