/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef __LIST_H
#define __LIST_H

typedef struct list {
    struct list *prev;
    struct list *next;
    void *value;
} list;

list *listNew(void);
void listRelease(list *l, void (*freevalue)(void *));
void listAppend(list *l, void *value);

#endif // !__LIST_H
