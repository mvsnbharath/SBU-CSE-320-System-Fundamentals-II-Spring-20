#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"

void assert_free_block_count(size_t size, int count);
void assert_free_list_block_count(size_t size, int count);

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
    sf_block *bp = sf_free_list_heads[i].body.links.next;
    while(bp != &sf_free_list_heads[i]) {
        if(size == 0 || size == (bp->header & BLOCK_SIZE_MASK))
        cnt++;
        bp = bp->body.links.next;
    }
    }
    if(size == 0) {
    cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
             count, cnt);
    } else {
    cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
             size, count, cnt);
    }
}

/*
 * Assert that the free list with a specified index has the specified number of
 * blocks in it.
 */
void assert_free_list_size(int index, int size) {
    int cnt = 0;
    sf_block *bp = sf_free_list_heads[index].body.links.next;
    while(bp != &sf_free_list_heads[index]) {
    cnt++;
    bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
         index, size, cnt);
}

Test(sf_memsuite_student, malloc_an_int, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    int *x = sf_malloc(sizeof(int));

    cr_assert_not_null(x, "x is NULL!");

    *x = 4;

    cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

    assert_free_block_count(0, 1);
    assert_free_block_count(3904, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);

    cr_assert(sf_errno == 0, "sf_errno is not zero!");
    cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    // We want to allocate up to exactly three pages.
    void *x = sf_malloc(3 * PAGE_SZ - ((1 << 6) - sizeof(sf_header)) - 64 - 2*sizeof(sf_header));

    cr_assert_not_null(x, "x is NULL!");
    assert_free_block_count(0, 0);
    cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_too_large, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    void *x = sf_malloc(PAGE_SZ << 16);

    cr_assert_null(x, "x is not NULL!");
    assert_free_block_count(0, 1);
    assert_free_block_count(65408, 1);
    cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    /* void *x = */ sf_malloc(8);
    void *y = sf_malloc(32);
    /* void *z = */ sf_malloc(1);

    sf_free(y);

    assert_free_block_count(0, 2);
    assert_free_block_count(64, 1);
    assert_free_block_count(3776, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    /* void *x = */ sf_malloc(8);
    void *y = sf_malloc(200);
    /* void *z = */ sf_malloc(1);

    sf_free(y);

    assert_free_block_count(0, 2);
    assert_free_block_count(256, 1);
    assert_free_block_count(3584, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_errno = 0;
    /* void *w = */ sf_malloc(8);
    void *x = sf_malloc(200);
    void *y = sf_malloc(300);
    /* void *z = */ sf_malloc(4);

    sf_free(y);
    sf_free(x);

    assert_free_block_count(0, 2);
    assert_free_block_count(576, 1);
    assert_free_block_count(3264, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *u = sf_malloc(200);
    /* void *v = */ sf_malloc(300);
    void *w = sf_malloc(200);
    /* void *x = */ sf_malloc(500);
    void *y = sf_malloc(200);
    /* void *z = */ sf_malloc(700);

    sf_free(u);
    sf_free(w);
    sf_free(y);

    assert_free_block_count(0, 4);
    assert_free_block_count(256, 3);
    assert_free_block_count(1600, 1);
    assert_free_list_size(3, 3);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);

    // First block in list should be the most recently freed block.
    int i = 3;
    sf_block *bp = sf_free_list_heads[i].body.links.next;
    cr_assert_eq(bp, (char *)y - 2*sizeof(sf_header),
             "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 2*sizeof(sf_header));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *x = sf_malloc(sizeof(int));
    /* void *y = */ sf_malloc(10);
    x = sf_realloc(x, sizeof(int) * 20);

    cr_assert_not_null(x, "x is NULL!");
    sf_block *bp = (sf_block *)((char *)x - 2*sizeof(sf_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Realloc'ed block size not what was expected!");

    assert_free_block_count(0, 2);
    assert_free_block_count(64, 1);
    assert_free_block_count(3712, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *x = sf_malloc(sizeof(int) * 20);
    void *y = sf_realloc(x, sizeof(int) * 16);

    cr_assert_not_null(y, "y is NULL!");
    cr_assert(x == y, "Payload addresses are different!");

    sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Block size not what was expected!");

    // There should be only one free block of size 3840.
    assert_free_block_count(0, 1);
    assert_free_block_count(3840, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *x = sf_malloc(sizeof(double) * 8);
    void *y = sf_realloc(x, sizeof(int));

    cr_assert_not_null(y, "y is NULL!");

    sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
    cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert((bp->header & BLOCK_SIZE_MASK) == 64, "Realloc'ed block size not what was expected!");

    // After realloc'ing x, we can return a block of size 64 to the freelist.
    // This block will go into the main freelist and be coalesced.
    assert_free_block_count(0, 1);
    assert_free_block_count(3904, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

// Case 1: Prev and Next are allocated
Test(sf_memsuite_student, free_if_both_prev_next_are_allocated, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_malloc(8);
    void*x =sf_malloc(8);
    sf_malloc(100);
    sf_free(x);
    assert_free_block_count(0, 2);
    assert_free_block_count(64, 1);
    assert_free_block_count(3712, 1);
    assert_free_list_size(0, 1);
    assert_free_list_size(0, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

// Case 2: If only next block is allocated
Test(sf_memsuite_student, free_if_only_next_is_allocated, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    void*x = sf_malloc(8);
    void*y =sf_malloc(8);
    sf_malloc(8);
    sf_free(x);
    sf_free(y);
    assert_free_block_count(0, 2);
    assert_free_block_count(128, 1);
    assert_free_block_count(3776, 1);
    assert_free_list_size(1, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

// Case 3: If only prev block is allocated
Test(sf_memsuite_student, free_if_only_prev_is_allocated, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_malloc(8);
    void*x =sf_malloc(8);
    sf_free(x);
    assert_free_block_count(0, 1);
    assert_free_block_count(3904, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

// Case 4: If both prev and next block are not allocated
Test(sf_memsuite_student, free_if_prev_and_next_are_not_allocated, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    void* x =sf_malloc(8);
    void*y =sf_malloc(8);
    sf_free(x);
    sf_free(y);
    assert_free_block_count(0, 1);
    assert_free_block_count(3968, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, mem_align_test, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_malloc(8);
    sf_memalign(500, 256);
    assert_free_block_count(0, 2);
    assert_free_block_count(3328, 1);
    assert_free_list_size(0, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, mem_align_basic_test, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_malloc(8);
    sf_memalign(500, 64);
    assert_free_block_count(0, 1);
    assert_free_block_count(3392, 1);
    assert_free_list_size(NUM_FREE_LISTS-1, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, invalid_align, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_memalign(500, 65);
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

Test(sf_memsuite_student, invalid_size, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    sf_memalign(500, 1);
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

// 4 conditions to check if a block to free is valid
//1) pointer is Null
Test(sf_memsuite_student, free_null, .init = sf_mem_init, .fini = sf_mem_fini,.signal = SIGABRT){
    sf_errno = 0;
    void* x = NULL;
    sf_free(x);
    cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

//2) pointer Not aligned to 64-byte boundary
Test(sf_memsuite_student, not_aligned, .init = sf_mem_init, .fini = sf_mem_fini,.signal = SIGABRT){
    sf_errno = 0;
    void* x = sf_malloc(8);
    sf_free(x+1);
    cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

//3) Allocated bit in the header is 0;
Test(sf_memsuite_student, free_already_freed_block, .init = sf_mem_init, .fini = sf_mem_fini,.signal = SIGABRT){
    sf_errno = 0;
    void *x = sf_malloc(8);
    sf_header* x_header = (void *)(((void*)(x) -sizeof(sf_header)));
    *x_header = *x_header & ~1;
    sf_free(x);
    cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

 // 4) prev_all bit and alloc bit from prev_block_header must be same
Test(sf_memsuite_student, prev_alloc_mismatch_while_freeing, .init = sf_mem_init
    ,.fini = sf_mem_fini,.signal = SIGABRT){
    sf_errno = 0;
    void *x = sf_malloc(8);
    void* y = sf_malloc(8);
    sf_free(x);
    sf_header* header_of_allocated = (sf_header*) ((void*)(y) - 8) ;
    sf_footer *prev_block_footer = (sf_footer*) ((void*) header_of_allocated-8);
    sf_header *header_of_prev_free = (sf_header*) ((void*)(prev_block_footer) -64 +8) ;
    *header_of_prev_free = 67;
    sf_free(y);
    sf_show_heap();
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

// 4 conditions to check if a re-alloc block is valid
//1) pointer is Null
Test(sf_memsuite_student, realloc_null_block, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    void* x =sf_realloc(NULL,12);
    cr_assert_null(x, "x is not NULL!");
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

//2) pointer Not aligned to 64-byte boundary
Test(sf_memsuite_student, realloc_not_aligned, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    void* x = sf_malloc(8);
    sf_realloc(x+1,64);
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

// 3) Allocated bit in the header is 0
Test(sf_memsuite_student, realloc_already_freed_block, .init = sf_mem_init, .fini = sf_mem_fini){
    sf_errno = 0;
    void *x = sf_malloc(8);
    sf_free(x);
    x = sf_realloc(x,100);
    cr_assert_null(x, "x is not NULL!");
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}

 // 4) prev_all bit and alloc bit from prev_block_header must be same
Test(sf_memsuite_student, realloc_if_prev_bit_header_mismatch, .init = sf_mem_init,.fini = sf_mem_fini){
    sf_errno = 0;
    void *x = sf_malloc(8);
    void* y = sf_malloc(8);
    sf_free(x);
    sf_header* header_of_allocated = (sf_header*) ((void*)(y) - 8) ;
    sf_footer *prev_block_footer = (sf_footer*) ((void*) header_of_allocated-8);
    sf_header *header_of_prev_free = (sf_header*) ((void*)(prev_block_footer) -64 +8) ;
    *header_of_prev_free = 67;
    y = sf_realloc(y,8);
    cr_assert(sf_errno == EINVAL, "sf_errno is not EINVAL!");
}