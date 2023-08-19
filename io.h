#ifndef IO_H
#define IO_H

#include "aostr.h"

aoStr *ioReadFile(char *path);
int ioWriteFile(char *path, char *data, int flags, ssize_t len);

#endif // !IO_H
