#include <assert.h>
#include <stddef.h>
#include "mm/buddy.h"

typedef struct buddy_t {
    int pool_size;

    // min alloc unit size
    int min_size;

    // allocate max & min order
    int min_order;
    int max_order;

    // buddy index
    int8_t* tree;
    int tree_size;

    // memory pool
    char* pool;
} buddy_t;

#define BUDDY_UNUSED 0
#define BUDDY_USED 1
#define BUDDY_SPLIT 2
#define BUDDY_FULL 3

// get memory size by index
#define BUDDY_SIZE_FROM_INDEX(buddy, index) \
    (buddy->pool_size >> ILOG2(index))

// get memory shift by index
#define BUDDY_SHIFT_FROM_INDEX(buddy, index) \
    (buddy->pool_size >> ILOG2(index )) * (index - (1 << ILOG2(index)))

// get memory by index
#define BUDDY_MEM_FROM_INDEX(buddy, index) \
    buddy->pool + BUDDY_SHIFT_FROM_INDEX(buddy, index)

#define BUDDY_MIN_SIZE 4

buddy_t*
buddy_init(size_t size, size_t min_alloc_size) {
    if (min_alloc_size < BUDDY_MIN_SIZE) {
        min_alloc_size = BUDDY_MIN_SIZE;
    }

    // round up size by 2^n
    size_t real_size = ROUNDUP(size);
    size_t real_min_size = ROUNDUP(min_alloc_size);
    if (real_min_size >= real_size) {
        goto BUDDY_FAIL;
    }

    buddy_t* buddy = (buddy_t*)MALLOC(sizeof(buddy_t));
    if (!buddy) {
        goto BUDDY_FAIL;
    }

    buddy->pool_size = real_size;
    buddy->min_size = real_min_size;
    buddy->max_order = ILOG2(buddy->pool_size) ;
    buddy->min_order = ILOG2(buddy->min_size);

    // alloc buddy index
    buddy->tree_size = buddy->pool_size * 2 / buddy->min_size;
    buddy->tree = (int8_t*)MALLOC(sizeof(int8_t) * buddy->tree_size);
    if (!buddy->tree) {
        goto BUDDY_FAIL1;
    }

    for(int index = 0; index < buddy->tree_size; index ++) {
        buddy->tree[index] = BUDDY_UNUSED;
    }

    // alloc memory pool
    buddy->pool = (char*)MALLOC(buddy->pool_size);
    if (!buddy->pool) {
        goto BUDDY_FAIL2;
    }

    // success
    return buddy;

BUDDY_FAIL2:
    FREE(buddy->tree);
BUDDY_FAIL1:
    FREE(buddy);
BUDDY_FAIL:
    return NULL;
}

int
buddy_release(buddy_t* buddy) {
    if (buddy) {
        FREE(buddy->tree);
        FREE(buddy->pool);
        FREE(buddy);
    }
    return 0;
}

void*
buddy_realloc(buddy_t* buddy, void* mem, size_t nbytes) {
    if (!buddy)
        return NULL;
    if (!mem)
        return buddy_alloc(buddy, nbytes);
    if (nbytes == 0) {
        buddy_free(buddy, mem);
        return NULL;
    }
    // calculate original size
#ifdef __x86_64__
    int offset = (int)((uint64_t)mem - (uint64_t)buddy->pool);
#else
    int offset = (int)((uint32_t)mem - (uint32_t)buddy->pool);
#endif
    assert(offset <= buddy->pool_size);
    int index = 1;
    int step = buddy->pool_size;
    int current = 0;
    size_t mem_size = 0;
    while (1) {
        if (buddy->tree[index] == BUDDY_USED) {
            mem_size = BUDDY_SIZE_FROM_INDEX(buddy, index);
            break;
        }
        // find dest by dichotomy
        step/= 2;
        index *= 2;
        if (offset >= current + step) {
            current += step;
            index ++;
        }
    }

    // alloc new memory
    void* new_mem = buddy_alloc(buddy, nbytes);
    if (new_mem) {
        memcpy(new_mem, mem, (mem_size < nbytes ? mem_size : nbytes));
    }
    buddy_free(buddy, mem);
    return new_mem;
}

void*
buddy_alloc(buddy_t* buddy, size_t nbytes) {
    if (!buddy)
        return NULL;
    // calculate malloc size
    size_t malloc_size = ROUNDUP(nbytes);
    if ((int)nbytes > buddy->pool_size)
        return NULL;
    if ((int)malloc_size < buddy->min_size)
        malloc_size = buddy->min_size;

#ifndef BUDDY_ALLOC_CHECK
#define BUDDY_ALLOC_CHECK(index) \
    if (index == 1) { \
        return NULL; \
    }

    // depth first search
    int index = 1;
    while (index < buddy->tree_size) {
        if (buddy->tree[index] == BUDDY_UNUSED) {
            size_t mem_size = BUDDY_SIZE_FROM_INDEX(buddy, index);
            // mark used
            if (mem_size == malloc_size) {
                buddy->tree[index] = BUDDY_USED;
                char* mem = BUDDY_MEM_FROM_INDEX(buddy, index);

                // loop back to set full flag
                while (index > 1) {
                    int left = ((index >> 1) << 1);
                    int right = left + 1;
                    if ((buddy->tree[left] == BUDDY_USED
                        || buddy->tree[left] == BUDDY_FULL)
                        && (buddy->tree[right] == BUDDY_USED
                        || buddy->tree[right] == BUDDY_FULL)) {
                        index /= 2;
                        buddy->tree[index] = BUDDY_FULL;
                        continue;
                    }
                    break;
                }
                return mem;
            // split and going down to get required 
            } else if (mem_size > malloc_size) {
                buddy->tree[index] = BUDDY_SPLIT;
                index *= 2;
                continue;
            // go back to parent's buddy
            } else {
                do { index /= 2; }
                while((index & 1) && (index > 1));
                BUDDY_ALLOC_CHECK(index);
                index ++;
            }
        } else if (buddy->tree[index] == BUDDY_USED
            || buddy->tree[index] == BUDDY_FULL) {
            // try self or parent's buddy
            while ((index & 1) && (index > 1)) {
                index /= 2;
            }
            BUDDY_ALLOC_CHECK(index);
            ++ index;
        } else if (buddy->tree[index] == BUDDY_SPLIT) {
            size_t mem_size = BUDDY_SIZE_FROM_INDEX(buddy, index);
            if (mem_size <= malloc_size) {
                // try self or parent's buddy
                while ((index & 1) && (index > 1)) {
                    index /= 2;
                }
                BUDDY_ALLOC_CHECK(index);
                ++ index;
            // go to child
            } else {
                index *= 2;
            }
        }
    }
#endif
    return NULL;
}

void
buddy_free(buddy_t* buddy, void* mem) {
    if (!mem || !buddy)
        return;
#ifdef __x86_64__
    int offset = (int)((uint64_t)mem - (uint64_t)buddy->pool);
#else
    int offset = (int)((uint32_t)mem - (uint32_t)buddy->pool);
#endif
    assert(offset <= buddy->pool_size);

    int index = 1;
    int step = buddy->pool_size;
    int current = 0;
    while (1) {
        if (buddy->tree[index] == BUDDY_USED) {
            assert(current == offset);
            buddy->tree[index] = BUDDY_UNUSED;

            // try merge buddy & set parent unused
            int loop = index;
            while (loop > 1) {
                int left = ((loop >> 1) << 1);
                int right = left + 1;
                if (buddy->tree[left] == BUDDY_UNUSED
                    && buddy->tree[right] == BUDDY_UNUSED) {
                    loop /= 2;
                    buddy->tree[loop] = BUDDY_UNUSED;
                    continue;
                }
                break;
            }

            // set full parent to split status
            loop = index;
            while (loop > 1) {
                loop /= 2;
                if (buddy->tree[loop] == BUDDY_FULL) {
                    buddy->tree[loop] = BUDDY_SPLIT;
                    continue;
                }
                break;
            }
            return;
        }

        // find dest by dichotomy
        step /= 2;
        index *= 2;
        if (offset >= current + step) {
            current += step;
            index ++;
        }
    }
}

static void
_buddy_debug_unit(buddy_t* buddy, int index) {
    int shift, size;
    assert(buddy);
    switch (buddy->tree[index]) {
        case BUDDY_UNUSED:
            shift = BUDDY_SHIFT_FROM_INDEX(buddy, index);
            size = BUDDY_SIZE_FROM_INDEX(buddy, index);
            printf("(%d:%d) ", shift, shift + size);
            break;

        case BUDDY_FULL:
            printf("{ ");
            _buddy_debug_unit(buddy, index * 2);
            _buddy_debug_unit(buddy, index * 2 + 1);
            printf("} ");
            break;

        case BUDDY_SPLIT:
            _buddy_debug_unit(buddy, index * 2);
            _buddy_debug_unit(buddy, index * 2 + 1);
            break;

        case BUDDY_USED:
            shift = BUDDY_SHIFT_FROM_INDEX(buddy, index);
            size = BUDDY_SIZE_FROM_INDEX(buddy, index);
            printf("[%d:%d] ", shift, shift + size);
            break;

        default:
            assert(0);
    }
}

void
buddy_debug(buddy_t* buddy) {
    if (buddy) {
        _buddy_debug_unit(buddy, 1);
        printf("\n");
    }
}

