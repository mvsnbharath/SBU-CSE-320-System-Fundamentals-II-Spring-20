/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "prologue_epilogue_struct_def.h"
#include "custom_helper_functions.h"
#include "circular_doubly_ll_helper_functions.h"

sf_prologue* prologue_block;
sf_epilogue* epilogue_block;

void* malloc_with_given_size(size_t total_block_size){
    int blockNumber = computeBlock(total_block_size);
    debug("Total Size: %ld, Block Number: %d",total_block_size,blockNumber);
    sf_block* free_block = get_sf_free_list_node(total_block_size, blockNumber);
    if(free_block!=NULL){
        return allocate_block_and_attach_free_block_to_the_list(free_block,total_block_size);
    }else{
        void* newPagePtr;
        while(free_block == NULL){
            newPagePtr = sf_mem_grow();
            if(newPagePtr == NULL){
                sf_errno = ENOMEM;
                return NULL;
            }

            //Old Epilogue
            sf_header *new_free_block_header = (sf_header*) ((void*)(epilogue_block) );

            // New Epilogue
            epilogue_block = (sf_epilogue *) (sf_mem_end() - sizeof(sf_epilogue));
            (*epilogue_block).header =  0 | 0x01;

            // New Free Block
            debug("check %p", new_free_block_header);
            int pa = get_last_block_allocation_status(new_free_block_header);
            debug("Epilogue Block :%p",epilogue_block);
            *new_free_block_header = PAGE_SZ | pa;
            sf_footer* new_free_block_footer = getFooterFromHeader(new_free_block_header);
            *new_free_block_footer =   *new_free_block_header;
            debug("Previous free block address: %p",getPrevBlockFooter(new_free_block_header));
            coallesce((sf_block *) getPrevBlockFooter(new_free_block_header));
            free_block = get_sf_free_list_node(total_block_size, blockNumber);
        }
        return allocate_block_and_attach_free_block_to_the_list(free_block,total_block_size);
    }
    return NULL;
}

void *sf_malloc(size_t size) {
     //When size is '0'
    if(size == 0)
        return NULL;

    // If start and end are same , heap is un-initialized (or) empty
    // Allocate some menory and initialize prologue and epilogue appropriately
    if(sf_mem_start() == sf_mem_end()){
        initialize_heap();
    }
     // Block Size and Free list Value Computation
    size_t total_block_size = size + sizeof(sf_header);
    if(total_block_size % ALIGNMENT !=0)
        total_block_size += (ALIGNMENT - (total_block_size % ALIGNMENT));

    return malloc_with_given_size(total_block_size);
}

void sf_free(void *pp) {
    validate_pointer(pp);
    sf_block *header_of_sf_block_to_free = (sf_block*) ((void*)(pp) -sizeof(sf_header)-sizeof(sf_footer)) ;
    sf_header *header_of_block_to_free = &(*header_of_sf_block_to_free).header;
    sf_footer *footer_of_block_to_free = getFooterFromHeader(header_of_block_to_free);

    // Change allocation bit fields in header and
    free_current_header_and_footer(header_of_block_to_free,footer_of_block_to_free);
    //Change Allocation bit in next block header and footer (or) epilogue (change only header)
    change_next_block_header_and_footer(footer_of_block_to_free);
    //coallesce
    coallesce(header_of_sf_block_to_free);
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    void* validation_check = validate_realloc_pointer(pp);
    if(validation_check == NULL){
        return NULL;
    }

    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }

    size_t prev_size = get_size(*(sf_header*) ((void*)(pp) -sizeof(sf_header)));
    if(prev_size == rsize){
        return pp;
    }else if(rsize > prev_size){
        void* new_ptr = sf_malloc(rsize);
        if(new_ptr == NULL)
            return NULL;
        memcpy(new_ptr,pp,rsize);
        sf_free(pp);
        return new_ptr;
    }else{
        size_t total_block_size_new_block= compute_size_with_alignment(rsize);
        size_t remaining_size = prev_size - total_block_size_new_block;

        if(remaining_size < BLOCK_SIZE){
            return pp;
        }else{
            //Allocated Block
            sf_header *pp_block_header = (sf_header*) ((void*)(pp) -sizeof(sf_header));
            int pa = is_previous_block_allocated(pp_block_header);
            *pp_block_header = total_block_size_new_block + pa + 1;
            sf_footer *pp_block_footer = getFooterFromHeader(pp_block_header);
            *pp_block_footer = *pp_block_header;

            //Free block
            sf_header *new_free_block_header = (sf_header*) ((void*)(pp_block_footer) +sizeof(sf_footer));
            *new_free_block_header = remaining_size | 0x02;
            sf_footer* new_free_block_footer = getFooterFromHeader(new_free_block_header);
            *new_free_block_footer =   *new_free_block_header;

            coallesce((sf_block *) pp_block_footer);
            return pp;
        }
    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    //If align is less than the minimum block size,then NULL is returned and
    //sf_errno is set to EINVAL.
    if (align<M){
        debug("Align: %ld is less than minimum block size",size);
        sf_errno = EINVAL;
        return NULL;
    }
    //If align is not a power of two then NULL is returned and sf_errno is set to EINVAL.
    if (isAlignmentPowerOfTwo(align)!=1){
        debug("Align: %ld is not a power of two",size);
        sf_errno = EINVAL;
        return NULL;
    }

    //If size is 0, then NULL is returned without setting sf_errno.
    if(size == 0){
        return NULL;
    }

    // If start and end are same , heap is un-initialized (or) empty
    // Allocate some menory and initialize prologue and epilogue appropriately
    if(sf_mem_start() == sf_mem_end()){
        initialize_heap();
    }

    size_t total_block_size = size + align + M + sizeof(sf_header);
    if(total_block_size % 64 !=0)
        total_block_size += (64 - (total_block_size % 64));


    size_t actualSizeRequired = size + sizeof(sf_header);
    if(actualSizeRequired % 64 !=0)
        actualSizeRequired += (64 - (actualSizeRequired % 64));

    void* raw_malloc = malloc_with_given_size(total_block_size);
    debug("Total block size requested %ld",total_block_size);
    debug("Raw Malloc %p",raw_malloc);
    sf_header* raw_malloc_header = (void *)(((void*)(raw_malloc) -sizeof(sf_header)));
    debug("Raw Malloc header %p",raw_malloc_header);
    debug("Size of Raw Malloc header %ld",get_size(*raw_malloc_header));
    debug("Actual Size with header: %ld ",actualSizeRequired);
    int prev_block_alloc_status = is_previous_block_allocated(raw_malloc_header);
    if (prev_block_alloc_status == 1){
        prev_block_alloc_status = 2;
    }
    debug("Prev Block alloc status %d",prev_block_alloc_status);
    int alignStatus = ((uintptr_t)raw_malloc % align);
    debug("Alignment Status: %d",alignStatus);
    if (alignStatus ==0){
        //block is aligned
        //check for empty space at the end and return
        int free_block_size_at_the_end = total_block_size - actualSizeRequired;
        if(free_block_size_at_the_end >= BLOCK_SIZE){ //This should always be true
            //change header value
            *raw_malloc_header = actualSizeRequired | (prev_block_alloc_status + 0x01);
            //Create free block header and footer
            sf_header* new_free_block_header = (sf_header*) (((void*)(raw_malloc_header)) +actualSizeRequired) ;
            *new_free_block_header = free_block_size_at_the_end | 0x02;
            sf_footer* new_free_block_footer = getFooterFromHeader(new_free_block_header);
            *new_free_block_footer = *new_free_block_header;
            debug("Allocated Block Header: %p",raw_malloc_header);
            debug("Free Block Header: %p",new_free_block_header);
            debug("Free Block Footer: %p",new_free_block_footer);
            //coallesce
            coallesce((sf_block *) getPrevBlockFooter(new_free_block_header));
            return raw_malloc;
        }

    }else{
        //move forward until aligned
        // free at the begining
        //alocate
        //free at the end
        //return

        //Initial free block at the begining
        int intital_free_block_size = align - alignStatus;
        debug("Initial free block size : %d",intital_free_block_size);
        sf_header* initial_free_block_header = raw_malloc_header;
        *initial_free_block_header = intital_free_block_size | prev_block_alloc_status;
        sf_footer* initial_free_block_footer = getFooterFromHeader(initial_free_block_header);
        *initial_free_block_footer = *initial_free_block_header;

        int ending_free_block_size = total_block_size - actualSizeRequired - intital_free_block_size;
        int is_there_a_free_block_at_the_end = 0;
        if(ending_free_block_size >=M){
            is_there_a_free_block_at_the_end = 1;
        }else{
            actualSizeRequired = total_block_size - intital_free_block_size;
        }
        //Allocated block
        sf_header* new_malloc_header = (sf_header*) (((void*)(raw_malloc_header)) +intital_free_block_size) ;
        *new_malloc_header = actualSizeRequired | 0x01;

        //Free Block ath the end
        if(is_there_a_free_block_at_the_end == 1){
            sf_header* free_block_header_at_the_end = (sf_header*) (((void*)(new_malloc_header)) +actualSizeRequired) ;
            *free_block_header_at_the_end = ending_free_block_size | 0x02;
            sf_footer* free_block_footer_at_the_end = getFooterFromHeader(free_block_header_at_the_end);
            *free_block_footer_at_the_end = *free_block_header_at_the_end;

            debug("Initial Free Block Header: %p",initial_free_block_header);
            debug("Initial Free Block Footer: %p",initial_free_block_footer);
            debug("Allocated Block Header: %p",new_malloc_header);
            debug("Final Free Block Header: %p",free_block_header_at_the_end);
            debug("Final Free Block Footer: %p",free_block_footer_at_the_end);

            //coallesce
            coallesce((sf_block *) getPrevBlockFooter(initial_free_block_header));
            coallesce((sf_block *) getPrevBlockFooter(free_block_header_at_the_end));
        }

        return new_malloc_header+1;
    }
    return NULL;
}
