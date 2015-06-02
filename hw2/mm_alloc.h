/*
 * mm_alloc.h
 *
 * Exports a clone of the interface documented in "man 3 malloc".
 */

#pragma once

#ifndef _malloc_H_
#define _malloc_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

void* mm_malloc(size_t size);
void* mm_realloc(void* ptr, size_t size);
void mm_free(void* ptr);


typedef struct s_block *s_block_ptr;

/* block struct */
struct s_block {
    struct s_block *next;
    struct s_block *prev;
    int free;
    size_t size;

    /* A pointer to the allocated block. You can use this to access the memory 
     * after this struct s_block, which would normally be what your mm_malloc()
     * will return to the user. This being a zero-length array makes it so
     * sizeof(struct s_block) doesn't include the variable length block.
     *
     * Note that zero-length arrays like this are a GCC extension and are not part
     * of standard C.
     */
    char data [0];
 };

/* Split block according to size, b must exist */
void split_block (s_block_ptr b, size_t s);

/* Try fusing block with neighbors */
s_block_ptr fusion(s_block_ptr b);

/* Get the block from addr */
s_block_ptr get_block (void *p);

/* Add a new block at the of heap,
 * return NULL if things go wrong
 */
s_block_ptr extend_heap (s_block_ptr last , size_t s);


#ifdef __cplusplus
}
#endif

#endif
