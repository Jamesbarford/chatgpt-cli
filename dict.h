#ifndef DICT_H
#define DICT_H

#include <stddef.h>
#include <stdlib.h>

#define DICT_INTITAL_CAPACITY (1 << 4)
#define DICT_LOAD             (0.75)

/* This is what will get stored in the hash table */
typedef struct dictNode {
    unsigned char *key;
    void *val;
    struct dictNode *next;
} dictNode;

typedef struct dictType {
    void (*freeKey)(void *);
    void (*freeValue)(void *);
    int (*keyCmp)(void *, void *);
    size_t (*hashFunction)(void *);
} dictType;

typedef struct dict {
    struct dict *next;
    long mask;
    long locked_flags;
    unsigned int size;
    unsigned int hash_mask;
    size_t capacity;
    size_t threashold;
    dictType *type;
    dictNode **body;
} dict;

#define dictSetHashFunction(d, fn) ((d)->type->hashFunction = (fn))
#define dictSetFreeKey(d, fn)      ((d)->type->freeKey = (fn))
#define dictSetFreeValue(d, fn)    ((d)->type->freeValue = (fn))
#define dictSetKeyCmp(d, fn)       ((d)->type->keyCmp = (fn))

#define dictHashFunction(d, k) ((d)->type->hashFunction(k))
#define dictFreeKey(d, k)      ((d)->type->freeKey ? d->type->freeKey(k) : (void)k)
#define dictFreeValue(d, k) \
    ((d)->type->freeValue ? d->type->freeValue(k) : (void)k)
#define dictKeyCmp(k1, k2) \
    ((d)->type->keyCmp ? d->type->keyCmp(k1, k2) : (k1 == k2))

void dictDefaultInit(dict *ht);
dict *dictNew(dictType *type);
void dictRelease(dict *d);
dictNode *dictFind(dict *d, void *key);
void *dictGet(dict *d, void *key);
size_t dictGenericHashFunction(void *key);
int dictDelete(dict *d, void *key);
int dictStrCmp(void *s1, void *s2);

/* For adding things */
int dictSet(dict *d, void *key, void *value);
void dictSetOrReplace(dict *d, void *key, void *value);

static dictType default_table_type = {
        .freeKey = free,
        .freeValue = NULL, /* TODO: not a clue what the value is */
        .keyCmp = dictStrCmp,
        .hashFunction = dictGenericHashFunction,
};

#endif
