/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"

#define dictShouldResize(d) ((d)->size >= (d)->threashold)

size_t dictGenericHashFunction(void *key) {
    char *s = (char *)key;
    size_t h = (size_t)*s;

    if (h) {
        for (++s; *s; ++s) {
            h = (h << 5) - h + (unsigned int)*s;
        }
    }

    return h;
}

int dictStrCmp(void *s1, void *s2) {
    return !strcmp(s1, s2);
}

void dictDefaultInit(dict *ht) {
    ht->type = &default_table_type;
    ht->capacity = DICT_INTITAL_CAPACITY;
    ht->hash_mask = ht->capacity - 1;
    ht->size = 0;
    ht->body = (dictNode **)calloc(ht->capacity, sizeof(dictNode *));
    ht->threashold = ~~((size_t)(DICT_LOAD * ht->capacity));
}

dict *dictNew(dictType *type) {
    dict *d = (dict *)malloc(sizeof(dict));
    d->type = type;
    d->capacity = DICT_INTITAL_CAPACITY;
    d->hash_mask = d->capacity - 1;
    d->size = 0;
    d->body = (dictNode **)calloc(d->capacity, sizeof(dictNode *));
    d->threashold = ~~((size_t)(DICT_LOAD * d->capacity));
    return d;
}

static dictNode *dictNodeNew(void *key, void *value) {
    dictNode *n = (dictNode *)malloc(sizeof(dictNode));
    n->val = value;
    n->key = key;
    n->next = NULL;
    return n;
}

void dictRelease(dict *d) {
    if (d) {
        dictNode *next = NULL;
        for (size_t i = 0; i < d->size; ++i) {
            dictNode *n = d->body[i];
            while (n) {
                next = n->next;
                dictFreeKey(d, n->key);
                dictFreeValue(d, n->val);
                free(n);
                d->size--;
                n = next;
            }
        }
        free(d);
    }
}

dictNode *dictFind(dict *d, void *key) {
    size_t hash = dictHashFunction(d, key);
    size_t idx = hash & d->hash_mask;
    dictNode *n = d->body[idx];

    for (; n != NULL; n = n->next) {
        if (dictKeyCmp(key, n->key)) {
            return n;
        }
    }

    return NULL;
}

void *dictGet(dict *d, void *key) {
    dictNode *needle = dictFind(d, key);
    return needle ? needle->val : NULL;
}

static size_t dictGetIndex(dict *d, void *key, size_t idx, int *ok) {
    dictNode *n = d->body[idx];

    for (; n != NULL; n = n->next) {
        if (dictKeyCmp(key, n->key)) {
            *ok = 0;
            return idx;
        }
    }
    *ok = 1;
    return idx;
}

static void dictResize(dict *d) {
    size_t new_capacity = d->capacity << 1;
    size_t new_mask = new_capacity - 1;
    size_t new_threashold = ~~((size_t)(DICT_LOAD * new_capacity));
    size_t size = d->size;
    dictNode **new_entries = (dictNode **)calloc(new_capacity,
                                                 sizeof(dictNode *));
    dictNode **old_entries = d->body;

    for (size_t i = 0; i < d->capacity && size > 0; ++i) {
        dictNode *n = old_entries[i];
        dictNode *next = NULL;
        size_t idx = 0;

        while (n) {
            idx = dictHashFunction(d, n->key) & new_mask;
            next = n->next;
            n->next = new_entries[idx];
            new_entries[idx] = n;
            size--;
            n = next;
        }
    }
    d->body = new_entries;
    d->hash_mask = new_mask;
    d->capacity = new_capacity;
    d->threashold = new_threashold;
    free(old_entries);
}

void dictSetOrReplace(dict *d, void *key, void *value) {
    if (dictShouldResize(d)) {
        dictResize(d);
    }

    size_t hash = dictHashFunction(d, key);
    size_t idx = hash & d->hash_mask;

    dictNode *n = d->body[idx];
    if (n == NULL) {
        dictNode *newnode = dictNodeNew(key, value);
        d->body[idx] = newnode;
        newnode->next = d->body[idx];
    } else {
        /* This could be a collision or we have the exact key and thus are
         * replacing */
        for (; n != NULL; n = n->next) {
            if (dictKeyCmp(key, n->key)) {
                /* Update the value */
                n->val = value;
                return;
            }
        }
        dictNode *newnode = dictNodeNew(key, value);
        newnode->next = d->body[idx];
        d->body[idx] = newnode;
    }
    d->size++;
}

int dictSet(dict *d, void *key, void *value) {
    if (dictShouldResize(d)) {
        dictResize(d);
    }

    size_t hash = dictHashFunction(d, key);
    size_t idx = hash & d->hash_mask;
    int ok = 0;

    idx = dictGetIndex(d, key, idx, &ok);

    /* Key exists and we do not want to update the value */
    if (ok == 0) {
        return 0;
    }

    dictNode *newnode = dictNodeNew(key, value);
    newnode->next = d->body[idx];
    d->body[idx] = newnode;
    d->size++;
    return 1;
}

int dictDelete(dict *d, void *key) {
    size_t hash = dictHashFunction(d, key);
    size_t idx = hash & d->hash_mask;
    dictNode *n = d->body[idx];
    dictNode *prev = NULL;

    for (; n != NULL; n = n->next) {
        if (dictKeyCmp(n->key, key)) {
            if (prev) {
                prev->next = n->next;
            } else {
                d->body[idx] = n->next;
            }
            d->size--;
            dictFreeKey(d, n->key);
            dictFreeValue(d, n->val);
            free(n);
            return 1;
        }
        prev = n;
    }
    return 0;
}
