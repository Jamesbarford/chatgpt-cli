/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

list *listNew(void) {
    list *l = (list *)malloc(sizeof(list));
    l->prev = l->next = l;
    l->value = NULL;
    return l;
}

void listAppend(list *l, void *data) {
    list *node = listNew();
    list *tail = l->prev;
    node->value = data;
    node->next = l;
    node->prev = tail;
    tail->next = node;
    l->prev = node;
}

void listRelease(list *l, void (*freevalue)(void *)) {
    if (l) {
        list *head = l;
        list *node = head->next;
        list *next = NULL;
        while (node != head) {
            next = node->next;
            if (freevalue) {
                freevalue(node->value);
            }
            free(node);
            node = next;
        }
        if (head) {
            free(head);
        }
    }
}
