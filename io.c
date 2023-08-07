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
