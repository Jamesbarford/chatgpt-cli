/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
static void commandChatList(openAiCtx *ctx, char *line);
static void commandChatRename(openAiCtx *ctx, char *line);
static void commandChatLoad(openAiCtx *ctx, char *line);
static void commandChatDel(openAiCtx *ctx, char *line);
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
        {"/save", commandSave},
        {"/autosave", commandAutoSave},
        {"/models", commandModels},
        {"/info", commandInfo},

        {"/system", commandSystem},
        {"/file", commandChatFile},

        {"/hist-list", commandChatHistoryList},
        {"/hist-del", commandChatHistoryDel},
        {"/hist-clear", commandChatHistoryClear},

        {"/chat-load", commandChatLoad},
        {"/chat-list", commandChatList},
        {"/chat-rename", commandChatRename},
        {"/chat-del", commandChatDel},

        {"/set-model", commandSetModel},
        {"/set-verbose", commandSetVerbose},
        {"/set-top_p", commandSetTopP},
        {"/set-presence-pen", commandSetPresencePenalty},
        {"/set-temperature", commandSetTemperature},

        {"/exit", commandExit},
        {"/help", commandHelp},
};

#define MAX_COMPLETIONS 10

typedef struct {
    char *command;
    char *hint;
    char *prompt;
    char *completions[MAX_COMPLETIONS];
    int num_completions; // to keep track of actual number of completions
} cliCommandInfo;

// Define the array of structs
cliCommandInfo cli_info[] = {
        {"/save", "/sa", " /save", {"/save"}, 1},
        {"/autosave", "/autos", " /autosave", {"/autosave"}, 1},
        {"/models", "/mod", " /models", {"/models"}, 1},
        {"/info", "/in", " /info", {"/info"}, 1},
        {"/system", "/sys", " /system <prompt>", {"/system"}, 1},
        {"/file", "/fi", " /file <file_path> <prompt>", {"/file"}, 1},

        {"/hist-list", "/hist-li", " /hist-list", {"/hist-list"}, 1},
        {"/hist-clear", "/hist-cl", " /hist-clear", {"/hist-clear"}, 1},
        {"/hist-del", "/hist-de", " /hist-del <id>", {"/hist-del"}, 1},
        {"/hist",
         "/hist",
         " /hist-<list | del | clear>",
         {"/hist-list", "/hist-del", "/hist-clear"},
         3},

        {"/chat-list", "/chat-li", " /chat-list", {"/chat-list"}, 1},
        {"/chat-load", "/chat-lo", " /chat-load <id>", {"/chat-load"}, 1},
        {"/chat-del", "/chat-de", " /chat-del <id>", {"/chat-del"}, 1},
        {"/chat-rename",
         "/chat-re",
         " /chat-rename <id> <name>",
         {"/chat-rename"},
         1},
        {"/chat",
         "/chat",
         " /chat-<load | list | rename | del>",
         {"/chat-load", "/chat-list", "/chat-rename", "/chat-del"},
         4},

        {"/set-model", "/set-m", " /set-model <model_id>", {"/set-model"}, 1},
        {"/set-verbose", "/set-v", " /set-verbose <1|0>", {"/set-verbose"}, 1},
        {"/set-top_p", "/set-to", " /set-top_p <float>", {"/set-top_p"}, 1},
        {"/set-presence-pen",
         "/set-pr",
         " /set-presence-pen <float>",
         {"/set-presence-pen"},
         1},
        {"/set-temperature",
         "/set-te",
         " /set-temperature <float>",
         {"/set-temperature"},
         1},
        {"/set",
         "/set",
         " /set-<model | verbose | top_p | presence-pen | temperature>",
         {"/set-model", "/set-verbose", "/set-top_p", "/set-presence-pen",
          "/set-temperature"},
         5},

        {"/exit", "/ex", " /exit", {"/exit"}, 1},
        {"/help", "/he", " /help", {"/help"}, 1},
};

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
        openAiCtxDbInit(ctx);
        openAiCtxDbNewChat(ctx);
        openAiCtxDbSaveHistory(ctx);
    }
    openAiCtxDbSaveHistory(ctx);
}

static void commandAutoSave(openAiCtx *ctx, char *line) {
    commandSave(ctx, line);
    openAiCtxSetFlags(ctx, OPEN_AI_FLAG_PERSIST);
}

int qsortAoStrCmp(const void *a, const void *b) {
    return aoStrCmp(*(aoStr **)a, *(aoStr **)b);
}

static void commandModels(openAiCtx *ctx, char *line) {
    (void)line;
    json *resp = openAiListModels(ctx);

    aoStr *buffer, *tmp;
    aoStr **arr = NULL;
    int arrlen = 0;

    if (resp) {
        json *data = jsonSelect(resp, ".data:a");
        if (data) {
            buffer = aoStrAlloc(2048);
            data = jsonGetArray(data);

            for (json *tmp = data; tmp != NULL; tmp = tmp->next) {
                json *id = jsonSelect(tmp, ".id:s");

                if (id) {
                    aoStrCat(buffer, id->str);
                    aoStrPutChar(buffer, ',');
                }
            }

            arr = aoStrSplit(buffer->data, ',', &arrlen);
            if (arr) {
                qsort(arr, arrlen, sizeof(aoStr *), qsortAoStrCmp);
                for (int i = 0; i < arrlen; ++i) {
                    tmp = arr[i];
                    if (tmp->len > 0) {
                        printf("%s\n", tmp->data);
                    }
                }
                aoStrArrayRelease(arr, arrlen);
            }
            aoStrRelease(buffer);
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
        prompt_warning("Usage: /file <file> <cmd>\n");
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

static void commandChatList(openAiCtx *ctx, char *line) {
    (void)line;
    list *chats = openAiCtxGetChats(ctx);
    list *node = chats->next;
    list *next = NULL;
    aoStr *buf = NULL;

    while (node != chats) {
        buf = node->value;
        next = node->next;
        printf("%s\n", buf->data);
        aoStrRelease(buf);
        free(node);
        node = next;
    }
    if (chats) {
        free(chats);
    }
}

static void commandChatDel(openAiCtx *ctx, char *line) {
    int id = 0;
    char *ptr = line;

    if (!isspace(*ptr)) {
        warning("Usage: /chat-del <id>\n");
        return;
    }

    ptr++;
    while (isdigit(*ptr)) {
        id = id * 10 + *ptr - '0';
    }
    openAiCtxDbDeleteChatById(ctx, id);
}

static void commandChatRename(openAiCtx *ctx, char *line) {
    char *bookend = NULL;
    int arrlen = 0;
    aoStr **commands = aoStrSplit(line, ' ', &arrlen);
    aoStr *name;

    if (commands == NULL || arrlen < 2) {
        warning("Usage: /chat-rename <id> <name_of_chat>\n");
        aoStrArrayRelease(commands, arrlen);
        return;
    }

    aoStr *str_int = commands[0];
    int id = (int)strtol(str_int->data, &bookend, 10);

    if (*bookend != '\0') {
        warning("/chat-rename failed to parse id as an int\n");
        aoStrArrayRelease(commands, arrlen);
        return;
    }

    name = aoStrAlloc(512);
    for (int i = 1; i < arrlen; ++i) {
        aoStrCatLen(name, commands[i]->data, commands[i]->len);
        aoStrPutChar(name, ' ');
    }

    openAiCtxDbRenameChat(ctx, id, name->data);
    aoStrArrayRelease(commands, arrlen);
    aoStrRelease(name);
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
            openAiCtxHistoryDel(ctx, msg_idx);
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
    fprintf(stderr, "\nCOMMANDS: \n\n");
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
    fprintf(stderr, "  chat-list - List chats saved in database\n");
    fprintf(stderr,
            "  chat-load <id> - Load a previously saved chat from database\n");
    fprintf(stderr, "  chat-del <id> - Delete a chat from database\n");
    fprintf(stderr,
            "  chat-rename <id> <name> - Rename a chat with id <id> to <name> in database\n");

    fprintf(stderr, "\nSET OPTIONS: \n\n");
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

static char *cliHintsCallback(const char *buf, int *color, int *bold) {
    *color = 90;
    *bold = 0;

    int len = sizeof(cli_info) / sizeof(cli_info[0]);
    int slen = 0;
    for (int i = 0; i < len; ++i) {
        cliCommandInfo *info = &cli_info[i];
        slen = strlen(info->hint);
        if (!strncmp(info->hint, buf, slen)) {
            return info->prompt;
        }
    }
    return NULL;
}

static void cliCompletionCallback(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == '/') {
        int len = sizeof(cli_info) / sizeof(cli_info[0]);
        int slen = 0;

        for (int i = 0; i < len; ++i) {
            cliCommandInfo *info = &cli_info[i];
            slen = strlen(info->hint);
            if (!strncmp(info->hint, buf, slen)) {
                for (int j = 0; j < info->num_completions; ++j) {
                    char *completion = info->completions[j];
                    linenoiseAddCompletion(lc, completion);
                }
                return;
            }
        }
    }
}

static void cliInit(char *filepath) {
    linenoiseHistoryLoad(filepath); /* Load the history at startup */
    linenoiseSetCompletionCallback(cliCompletionCallback);
    linenoiseSetHintsCallback(cliHintsCallback);
    linenoiseSetMultiLine(1);
}

static dict *cliLoadCommands(void) {
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

void cliMain(openAiCtx *ctx) {
    char *line, *ptr;
    aoStr *history_filepath;
    struct passwd *pw = getpwuid(getuid());

    if (pw == NULL) {
        panic("could not get current working directory: %s\n", strerror(errno));
    }

    dict *commands = cliLoadCommands();
    openAiCommand *command;
    char cmd[128];
    int cmd_len = 0;

    history_filepath = aoStrAlloc(256);
    aoStrCatPrintf(history_filepath, "%s/.chatgpt-cli-hist.txt", pw->pw_dir);
    cliInit(history_filepath->data);

    while (1) {
        line = linenoise(">>> ");
        cmd_len = 0;
        if (line == NULL) {
            break;
        }
        ptr = line;

        if (line[0] != '\0' && line[0] != '/') {
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(history_filepath->data);
            commandChat(ctx, line);
        } else if (line[0] != '\0' && line[0] == '/') {
            while (!isspace(*ptr) && *ptr != '\0') {
                cmd[cmd_len++] = *ptr++;
            }
            cmd[cmd_len] = '\0';
            command = dictGet(commands, cmd);

            if (!command) {
                warning("Command: %s not found\n", ptr);
            } else {
                command->commandHandler(ctx, ptr);
                linenoiseHistoryAdd(line);
                linenoiseHistorySave(history_filepath->data);
            }
        }
        free(line);
    }
}
