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
#include "cli.h"

char *getApiKey(void) {
    char *apikey = NULL;
    int fd;

    if ((apikey = getenv("OPENAI_API_KEY")) != NULL) {
        return apikey;
    }

    if ((fd = open(".env", O_RDONLY, 0666)) == -1) {
        panic("Failed to read .env file and OPENAI_API_KEY was not set as an environment variable\n");
    }

    off_t len = lseek(fd, 0, SEEK_END);
    if (len == 0) {
        panic("Failed to read .env file and get OPENAI_API_KEY, file has no contents\n");
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

int main(void) {
    char *apikey = getApiKey();
    openAiCtx *ctx = openAiCtxNew(apikey, "gpt-3.5-turbo", NULL);

    openAiCtxSetFlags(ctx, (OPEN_AI_FLAG_HISTORY | OPEN_AI_FLAG_STREAM));
    openAiCtxDbInit(ctx);
    cliMain(ctx);
}
