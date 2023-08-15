#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aostr.h"
#include "dict.h"
#include "io.h"
#include "json-selector.h"
#include "json.h"
#include "linenoise.h"
#include "openai.h"
#include "panic.h"

typedef void commandHandlerFunction(openAiCtx *ctx, char *line);

typedef struct openAiCommand {
    char *name;
    commandHandlerFunction *commandHandler;
} openAiCommand;

static void commandChat(openAiCtx *ctx, char *line);
static void commandSave(openAiCtx *ctx, char *line);
static void commandAutoSave(openAiCtx *ctx, char *line);
static void commandModels(openAiCtx *ctx, char *line);
static void commandSystem(openAiCtx *ctx, char *line);
static void commandChatFile(openAiCtx *ctx, char *line);
static void commandPersist(openAiCtx *ctx, char *line);
static void commandChatList(openAiCtx *ctx, char *line);
static void commandChatRename(openAiCtx *ctx, char *line);
static void commandChatLoad(openAiCtx *ctx, char *line);
static void commandChatHistoryList(openAiCtx *ctx, char *line);
static void commandChatHistoryClear(openAiCtx *ctx, char *line);
static void commandChatHistoryDel(openAiCtx *ctx, char *line);
static void commandExit(openAiCtx *ctx, char *line);
static void commandHelp(openAiCtx *ctx, char *line);
static void commandInfo(openAiCtx *ctx, char *line);
static void commandSetModel(openAiCtx *ctx, char *line);
static void commandSetVerbose(openAiCtx *ctx, char *line);
static void commandSetTopP(openAiCtx *ctx, char *line);
static void commandSetPresencePenalty(openAiCtx *ctx, char *line);
static void commandSetTemperature(openAiCtx *ctx, char *line);

static openAiCommand readonly_command[] = {
        {"", commandChat},
        {"save", commandSave},
        {"autosave", commandAutoSave},
        {"models", commandModels},
        {"info", commandInfo},

        {"system", commandSystem},
        {"file", commandChatFile},
        {"hist-clear", commandChatHistoryClear},
        {"hist-list", commandChatHistoryList},
        {"hist-del", commandChatHistoryDel},
        {"chat-load", commandChatLoad},
        {"chat-list", commandChatList},
        {"set-model", commandSetModel},
        {"set-verbose", commandSetVerbose},
        {"set-top_p", commandSetTopP},
        {"set-presence-pen", commandSetPresencePenalty},
        {"set-temperature", commandSetTemperature},

        {"exit", commandExit},
        {"help", commandHelp},
};

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
    char *ptr = line; /* skip /system*/
    if (*ptr == '\0') {
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

static void commandChatFile(openAiCtx *ctx, char *line) {
    aoStr *cmdbuffer = aoStrAlloc(2048);
    char *ptr = line; /* skip /file */
    char path[BUFSIZ], cmd[BUFSIZ];
    ssize_t pathlen = 0, cmdlen = 0;
    aoStr *file_contents;

    if (*ptr == '\0' || !isspace(*ptr)) {
        prompt_warning("Usage: /file <file> <cmd>\n");
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
    ptr = line;
    if (!isspace(*ptr)) {
        warning("Usage: /chat-rename <name_of_chat>\n");
        return;
    }
    ptr++;
    ;
    while (*ptr != '\0') {
        newname[newlen++] = *ptr++;
    }
    newname[newlen] = '\0';
    openAiCtxDbRenameChat(ctx, ctx->chat_id, newname);
}

/* Can load a chat from the database without saving subsequent messages to that
 * chat */
static void commandChatLoad(openAiCtx *ctx, char *line) {
    char *ptr = line;
    int chat_id = -1;

    if (!isspace(*ptr)) {
        warning("Usage: /chat-load <chat_id>\n");
        return;
    }

    ptr++;

    while (!isdigit(*ptr) && *ptr != '\0') {
        ptr++;
    }
    if (*ptr == '\0') {
        warning("Usage: /chat-load <chat_id>\n");
    } else {
        chat_id = atoi(ptr);
        if (chat_id) {
            openAiCtxLoadChatHistoryById(ctx, chat_id);
        }
    }
}

static void commandChatHistoryList(openAiCtx *ctx, char *line) {
    (void)line;
    printf("messages: %d\n", ctx->chat_len);
    openAiCtxHistoryPrint(ctx);
}

static void commandChatHistoryDel(openAiCtx *ctx, char *line) {
    char *ptr = line;
    char *ok;
    int msg_idx = -1;

    if (!isspace(*ptr)) {
        prompt_warning("Usage: /hist-del <msg_idx>\n");
        return;
    }

    while (!isdigit(*ptr) && *ptr != '\0') {
        ptr++;
    }
    if (*ptr == '\0') {
        prompt_warning("Usage: /hist-del <msg_idx>\n");
    } else {
        msg_idx = (int)strtol(ptr, &ok, 10);
        if (msg_idx != -1) {
            printf("%d\n", msg_idx);
            openAiCtxHistoryDel(ctx,msg_idx);
        }
    }
}

static void commandSetModel(openAiCtx *ctx, char *line) {
    char *ptr = line;
    char model[BUFSIZ];
    ssize_t len = 0;
    if (!isspace(*ptr)) {
        prompt_warning("Usage: /set-model <model_name>\n");
        return;
    }
    ptr++;
    while (*ptr != '\0') {
        model[len++] = *ptr++;
    }
    model[len] = '\0';
    openAiCtxSetModel(ctx, model);
}

static void commandExit(openAiCtx *ctx, char *line) {
    if (ctx->flags & OPEN_AI_FLAG_PERSIST) {
        commandSave(ctx, line);
    }
    fprintf(stderr, "Good bye!\n");
    exit(EXIT_SUCCESS);
}

static void commandHelp(openAiCtx *ctx, char *line) {
    printf("Helping");
    fprintf(stderr, "COMMANDS: \n\n");
    fprintf(stderr, "  save - Saves current chat to SQLite3 database\n");
    fprintf(stderr,
            "  autosave - Saves current chat to SQLite3 database and will save all future messages both to and from GPT\n");

    fprintf(stderr, "  models - Lists all openai models avalible to you\n");
    fprintf(stderr, "  info - Lists all current configured options\n");
    fprintf(stderr,
            "  system <cmd> - Write a system message, has a massive impact on how GPT behaves\n");
    fprintf(stderr,
            "  file <file_path> <cmd> - Load in a file and ask GPT about it!\n");
    fprintf(stderr, "  hist-list - List current chat history\n");
    fprintf(stderr,
            "  hist-del <msg_idx> - Delete a specific message from memory\n");
    fprintf(stderr,
            "  hist-clear - Clear all history from memory, but not SQLite3\n");
    fprintf(stderr,
            "  chat-load <id> - Load a previously saved chat from SQLite3\n");

    fprintf(stderr, "SET OPTIONS: \n\n");
    fprintf(stderr,
            "  set-verbose <1|0> - Prints HTTP information, streams and debug info\n");
    fprintf(stderr, "  set-model <name> - Swith the currently used model\n");
    fprintf(stderr,
            "  set-top_p <float> - Set nucleus sampling, where the model considers the results of the tokens with top_p probability mass\n");
    fprintf(stderr,
            "  set-presence-pen <float> - 2.0 - 2.0 Positives penalize tokens if they have already appeared in the text\n");
    fprintf(stderr,
            "  set-temperature <float> - 0.0 - 2.0 Higher values will make the output more random\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "  exit - Exits program\n");
    fprintf(stderr, "  help - Displays this message\n");
}

static void commandInfo(openAiCtx *ctx, char *line) {
    (void)line;
    openAiCtxPrint(ctx);
}

static void commandSetVerbose(openAiCtx *ctx, char *line) {
    char *ptr = line;

    if (!isspace(*ptr)) {
        warning("Usage: set-verbose <1|0>\n");
        return;
    }
    ptr++;
    if (*ptr == '1') {
        ctx->flags |= OPEN_AI_FLAG_VERBOSE;
    } else if (*ptr == '0') {
        ctx->flags ^= OPEN_AI_FLAG_VERBOSE;
    } else {
        warning("set-verbose '%c' is invalid\n", *ptr);
    }
}

static void commandSetTopP(openAiCtx *ctx, char *line) {
    char *ptr = line, *check;
    float top_p = 0;
    if (!isspace(*ptr)) {
        warning("Usage: set-top_p <float>\n");
        return;
    }
    ptr++;
    top_p = strtof(ptr, &check);
    if (check != NULL) {
        warning("Usage: set-top_p <float>\n");
        return;
    }
    openAiCtxSetTopP(ctx, top_p);
}

static void commandSetPresencePenalty(openAiCtx *ctx, char *line) {
    char *ptr = line, *check;
    double presence_penalty = 0;
    if (!isspace(*ptr)) {
        warning("Usage: set-presence-pen <float>\n");
        return;
    }
    ptr++;
    presence_penalty = strtof(ptr, &check);
    if (check != NULL) {
        warning("Usage: set-presence-pen <float>\n");
        return;
    }
    if (presence_penalty < -2.0 || presence_penalty > 2.0) {
        warning("Usage: set-presence-pen must be between -2.0 and 2.0 '%g' given\n",
                presence_penalty);
    } else {
        openAiCtxSetPresencePenalty(ctx, presence_penalty);
    }
}

static void commandSetTemperature(openAiCtx *ctx, char *line) {
    char *ptr = line, *check;
    float temperature = 0;
    if (!isspace(*ptr)) {
        warning("Usage: set-temperature <float>\n");
        return;
    }
    ptr++;
    temperature = strtof(ptr, &check);
    if (check != NULL) {
        warning("Usage: set-temperature <float>\n");
        return;
    }
    if (temperature < 0.0 || temperature > 2.0) {
        warning("Usage: set-temperature must be between 0.0 and 2.0 '%g' given\n",
                temperature);
    } else {
        openAiCtxSetTemperature(ctx, temperature);
    }
}

static void promptInit(void) {
    linenoiseHistoryLoad("history.txt"); /* Load the history at startup */
    linenoiseSetCompletionCallback(promptCompletion);
    linenoiseSetHintsCallback(promptHints);
    linenoiseSetMultiLine(1);
}

static dict *promptLoadCommands(void) {
    dict *commands = dictNew(&default_table_type);
    dictSetFreeKey(commands, NULL);
    openAiCommand *command;
    int len = sizeof(readonly_command) / sizeof(readonly_command[0]);
    for (int i = 0; i < len; ++i) {
        command = &readonly_command[i];
        assert(dictSet(commands, command->name, command) == 1);
    }
    return commands;
}

void promptMain(openAiCtx *ctx) {
    char *line, *ptr;
    double num = 0.0;
    promptInit();
    dict *commands = promptLoadCommands();
    openAiCommand *command;
    char cmd[128];
    int cmd_len = 0;

    while (1) {
        line = linenoise(">>> ");
        cmd_len = 0;
        if (line == NULL) {
            break;
        }
        ptr = line;

        if (line[0] != '\0' && line[0] != '/') {
            linenoiseHistoryAdd(line);           /* Add to the history. */
            linenoiseHistorySave("history.txt"); /* Save the history on disk. */
            commandChat(ctx, line);
        } else if (line[0] != '\0' && line[0] == '/') {
            ptr += 1;
            
            while (!isspace(*ptr) && *ptr != '\0') {
                cmd[cmd_len++] = *ptr++;
            }
            cmd[cmd_len] = '\0';
            command = dictGet(commands, cmd);

            if (!command) {
                warning("Command: %s not found\n", ptr);
            } else {
                command->commandHandler(ctx, ptr);
            }
        }
        free(line);
    }
}
