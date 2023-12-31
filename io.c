/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <sys/fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"
#include "aostr.h"
#include "panic.h"

aoStr *ioReadFile(char *path) {
    aoStr *str = NULL;
    char *buffer = NULL;
    int fd = -1;
    ssize_t len = 0;

    fd = open(path, O_RDONLY, 0666);

    if (fd == -1) {
        warning("Failed to open file: %s\n", strerror(errno));
        return NULL;
    }

    len = lseek(fd, 0, SEEK_END);
    if (len <= 0) {
        warning("lseek failed to get length of file: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);

    buffer = malloc(sizeof(char) * len);

    if (buffer == NULL) {
        warning("Possible OOM: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }

    if (read(fd, buffer, len) != len) {
        warning("Failed to read all of file: %s\n", strerror(errno));
        free(buffer);
        close(fd);
        return NULL;
    }
    buffer[len] = '\0';

    close(fd);

    str = aoStrFromString(buffer, len);

    return str;
}

int ioWriteFile(char *path, char *data, int flags, ssize_t len) {
    int fd = open(path, flags, 0644);
    int nwritten = 0, towrite = len, total = 0;

    if (fd == -1) {
        warning("Could not open '%s': %s\n", path, strerror(errno));
        return 0;
    }

    while ((nwritten = write(fd, data, towrite)) > 0) {
        towrite -= nwritten;
        total += nwritten;
    }

    if (nwritten < 0) {
        warning("Write failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    close(fd);
    return total == len;
}
