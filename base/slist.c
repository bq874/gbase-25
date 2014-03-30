#include "slist.h"

typedef struct slist_node_t {
    struct slist_node_t* next;
    void* data;
} slist_node_t;

typedef struct slist_t {
    int size;
    slist_node_t* node;
} slist_t;

slist_t*
slist_create() {
    slist_t* sl = (slist_t*)MALLOC(sizeof(slist_t));
    if (!sl) return NULL;
    sl->node = NULL;
    sl->size = 0;
    return sl;
}

void
slist_release(slist_t* sl) {
    if (sl) {
        slist_clean(sl);
        FREE(sl);
    }
}

// more effective than push_back as it's single list
int
slist_push_front(slist_t* sl, void* data) {
    slist_node_t* new_node;
    if (!sl || !data) return -1;

    new_node = (slist_node_t*)MALLOC(sizeof(*new_node));
    if (!new_node) return -1;
    new_node->data = data;
    new_node->next = sl->node;
    sl->size ++;
    sl->node = new_node;
    return 0;
}

int
slist_push_back(slist_t* sl, void* data) {
    slist_node_t* new_node;
    slist_node_t* node;
    if (!sl || !data) return -1;

    new_node = (slist_node_t*)MALLOC(sizeof(*new_node));
    if (!new_node) return -1;
    new_node->data = data;
    new_node->next = 0;
    sl->size ++;

    if (!sl->node) {
        sl->node = new_node;
    } else {
        node = sl->node;
        while (node->next) {
            node = node->next;
        }
        node->next = new_node;
    }
    return 0;
}

// more effective than pop_back, as it's single list
void*
slist_pop_front(slist_t* sl) {
    slist_node_t* n;
    void* data;
    if (!sl || !sl->node) {
        return NULL;
    }
    n = sl->node;
    data = n->data;
    sl->node = n->next;
    FREE(n);
    sl->size --;
    return data;
}

void*
slist_pop_back(slist_t* sl) {
    slist_node_t* prev;
    slist_node_t* n;
    void* data;
    if (!sl || !sl->node) {
        return NULL;
    }
    n = sl->node;
    prev = NULL;
    while (n->next) {
        prev = n;
        n = n->next;
    }
    if (!prev) {
        sl->node = NULL;
    } else {
        prev->next = NULL;
    }
    data = n->data;
    FREE(n);
    sl->size --;
    return data;
}

int
slist_remove(slist_t* sl, void* data) {
    slist_node_t *node, *tmp;
    if (!sl || !data) return -1;

    node = sl->node;
    tmp = 0;
    while (node && node->data != data) {
        tmp = node;
        node = node->next;
    }
    if (node) {
        if (0 == tmp) {
            sl->node = node->next;
        } else {
            tmp->next = node->next;
        }
        FREE(node);
        sl->size --;
    }
    return 0;
}

int
slist_find(slist_t* sl, void* data) {
    slist_node_t* node;
    if (!sl || !data) return -1;
    node = sl->node;
    while (node) {
        if (node->data == data) {
            return 0;
        }
        node = node->next;
    }
    return -1;
}

int
slist_clean(slist_t* sl) {
    slist_node_t *node, *tmp;
    if (!sl) return -1;
    node = sl->node;
    while (node) {
        tmp = node->next;
        FREE(node);
        node = tmp;
    }
    sl->node = 0;
    return 0;
}

int
slist_size(slist_t* sl) {
    return sl ? sl->size : -1;
}

