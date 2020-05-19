#include <stdio.h>
#include <errno.h>
#include "sfmm.h"
#include "debug.h"
#include "prologue_epilogue_struct_def.h"
#include "custom_helper_functions.h"
#include "circular_doubly_ll_helper_functions.h"

extern sf_prologue* prologue_block;
extern sf_epilogue* epilogue_block;
extern int removed_from_free_list_block;

void initialize_heap(){

    debug("Initially heap is empty, allocate some memory");
    void* heap_begin = sf_mem_grow();

    if(heap_begin == NULL){
        debug("Memory allocation failed");
        sf_errno =  ENOMEM;
    }
    debug("Start of Heap: %p",sf_mem_start() );
    debug("End of Heap  : %p",sf_mem_end() );
    debug("%ld bytes of memory has been allocated to the heap",sf_mem_end() - sf_mem_start() );

    //Initially leave 7*8 = 56 bytes
    //Set Prologue
    prologue_block = (sf_prologue *) ((void *)(heap_begin + 56));
    debug("Prologue Header: %p",prologue_block);
    // Set the 2 LSB  as 1
    (*prologue_block).header =  64 | 0x03;
    (*prologue_block).footer = (*prologue_block).header;

    // Set Epilogue
    epilogue_block = (sf_epilogue *) (sf_mem_end() - sizeof(sf_epilogue));
    (*epilogue_block).header =  0 | 0x01;
    debug("Epilogue block has been set: %p",epilogue_block);

    size_t remaining_bytes =  sf_mem_end()-sf_mem_start()-56-BLOCK_SIZE-sizeof(sf_epilogue);
    debug("Remaining Space: %ld", remaining_bytes);

    //Wilderness is always at the end
    int blockNumber = NUM_FREE_LISTS-1;

    // Initially when there is no memory
    initialize_all_free_lists();
    // debug("Initialized All %d lists in sf_free_list_heads",NUM_FREE_LISTS);

    sf_header* free_block_header = (sf_header*) (heap_begin + 120);
    sf_footer* free_block_footer = (sf_footer *)(sf_mem_end() -8-8);
    *free_block_header = remaining_bytes | 0x2;
    *free_block_footer =   *free_block_header ;
    debug("Header Address of 1st block: %p",free_block_header);
    debug("Footer Address of 1st block: %p",free_block_footer);
    debug("Free Block Header Size: %ld",get_size(*free_block_header));

    sf_block* current_sf_block = (sf_block*) (heap_begin + 120 - sizeof(sf_footer));

    add_to_free_circular_dll(current_sf_block,blockNumber);
    debug("Heap Initialization complete");
    debug("=============================");
    return;
}

void* allocate_block_and_attach_free_block_to_the_list(sf_block* free_block,size_t total_block_size){

    //Allocated
    free_block->header = free_block->header | 0x1;
    size_t remaining_bytes_after_allocation = get_size(free_block -> header)-total_block_size;

    if (remaining_bytes_after_allocation>=0 && remaining_bytes_after_allocation<BLOCK_SIZE){
        return &free_block->header +1;
    }
    else if(remaining_bytes_after_allocation>=BLOCK_SIZE){

        //Allocated Block
        sf_header *allocated_block_header = &(*free_block).header;
        int pa = is_previous_block_allocated(allocated_block_header);
        *allocated_block_header = total_block_size | (pa+0x01);

        // New Free block
        if(remaining_bytes_after_allocation!=0){

            //previous block is always allocated
            sf_header *new_free_block_header = (sf_header*) ((void*)(allocated_block_header) +get_size(free_block -> header));
            *new_free_block_header = remaining_bytes_after_allocation | 0x02;
            sf_footer* new_free_block_footer = getFooterFromHeader(new_free_block_header);
            *new_free_block_footer =   *new_free_block_header;
            debug("Allocated Block Header: ");

            //Add this to the free list block
            sf_block* free_sf_block = (void*) (new_free_block_header) - sizeof(sf_footer);
            int new_free_list_block = computeBlock(remaining_bytes_after_allocation);
            debug("New free list block: %d",new_free_list_block);
            if (removed_from_free_list_block ==9){
                new_free_list_block = 9;
                removed_from_free_list_block = -1;
            }
            add_to_free_circular_dll(free_sf_block,new_free_list_block);
        }

        // Check if epilogue has reached
        if((void *)(allocated_block_header) + total_block_size == (sf_mem_end() - sizeof(sf_epilogue))){
            (*epilogue_block).header =  0 | 0x03;
        }
        return allocated_block_header +1;
    }else{
        return NULL;
    }
}

size_t compute_size_with_alignment(size_t size){
    size_t total_block_size__new_block = size + sizeof(sf_header);
    if(total_block_size__new_block % ALIGNMENT !=0){
        total_block_size__new_block += (ALIGNMENT - (total_block_size__new_block % ALIGNMENT));
    }
    return total_block_size__new_block;
}

int computeBlock(size_t size){
    if (size == M)
        return 0;
    else if (size == 2*M)
        return 1;
    else if (size == 3*M)
        return 2;
    else if (size > 3*M && size <= 5*M)
        return 3;
    else if (size > 5*M && size <= 8*M)
        return 4;
    else if (size > 8*M && size <= 13*M)
        return 5;
    else if (size > 13*M && size <= 21*M)
        return 6;
    else if (size > 21*M && size <= 34*M)
        return 7;
    else if (size > 34*M)
        return 8;
    else
        return 9;
}

size_t get_size(size_t size){
    return size & BLOCK_SIZE_MASK ;
}

int is_previous_block_allocated(sf_header *block_header){
    return  (*block_header & 0x02);
}

int is_block_allocated(sf_header *block_header){
    return (*block_header & 0x01);
}

int get_last_block_allocation_status(sf_header* block_header){
    //check epilogue previous block status
    if (is_previous_block_allocated(block_header) == 0x02){
        return 0x02;
    }else{
        return 0;
    }
}

sf_footer* getFooterFromHeader(sf_header* header){
    return (sf_footer*) (((void*)(header) +get_size(*header)) -sizeof(sf_footer)) ;
}

sf_header* getHeaderFromFooter(sf_footer* footer){
    return (sf_header*) ((void*)(footer) -get_size((*footer)) +sizeof(sf_footer)) ;
}

sf_footer* getPrevBlockFooter(sf_header* header){
    return (sf_footer*) (((void*)(header)) -sizeof(sf_footer)) ;
}

void validate_pointer(void *pp){

    //1) pointer is Null
    if(pp == NULL)
        abort();

    //2) pointer Not aligned to 64-byte boundary
    int addresss = ((uintptr_t)pp % 64);
    debug("addresss %d ",addresss);
    if (addresss!=0)
        abort();

    //3) Allocated bit in the header is 0;
    sf_header *header_of_block_to_free = (sf_header*) (pp-8);
    debug("Header of block to be freed %p",header_of_block_to_free);
    int header_value = is_block_allocated(header_of_block_to_free);
    debug("Block to be freed allocated bit: %d",header_value);
    if(header_value != 1)
        abort();

    // 4a) header is before end of prologue
    if((void *) header_of_block_to_free < ((void*)(sf_mem_start() + 120))){
        debug("Header is before end of prologue");
        abort();
    }

    // 4b) Footer is after begining of epilogue
    sf_footer *footer_of_block_to_free =  getFooterFromHeader(header_of_block_to_free);
    debug("Footer of the block to be freed : %p",footer_of_block_to_free);
    if((void *)footer_of_block_to_free >= (void*)(sf_mem_end()) - sizeof(sf_epilogue)){
        debug("Footer is after begining of epilogue");
        abort();
    }

    // 5) prev_all bit and alloc bit from prev_block_header must be same
    int prev_field = is_previous_block_allocated(header_of_block_to_free);
    if (prev_field == 0){
        sf_footer *prev_block_footer = (sf_footer*) ((void*) header_of_block_to_free-8);
        sf_header *prev_block_header = getHeaderFromFooter(prev_block_footer);
        debug("Size of Prev Block: %ld, Prev Header: %p , Prev Footer: %p",
            get_size(*prev_block_footer),prev_block_header,prev_block_footer);
        int is_allocated_from_prev_block_header = is_block_allocated(prev_block_header);
        debug("Check: %d %ld",is_allocated_from_prev_block_header, *prev_block_footer);
        if(is_allocated_from_prev_block_header!=0){
            debug("prev_alloc fields is 0 but the alloc field of the prev header is not zero");
            abort();
        }
    }
    return;
}

void* validate_realloc_pointer(void *pp){

    //1) pointer is Null
    if(pp == NULL){
        sf_errno = EINVAL;
        return NULL;
    }

    //2) pointer Not aligned to 64-byte boundary
    int addresss = ((uintptr_t)pp % 64);
    debug("addresss %d ",addresss);
    if (addresss!=0){
        sf_errno = EINVAL;
        return NULL;
    }

    //3) Allocated bit in the header is 0;
    sf_header *header_of_block_to_free = (sf_header*) (pp-8);
    debug("Header of block to be freed %p",header_of_block_to_free);
    int header_value = is_block_allocated(header_of_block_to_free);
    debug("Block to be freed allocated bit: %d",header_value);
    if(header_value != 1){
        sf_errno = EINVAL;
        return NULL;
    }

    // 4a) header is before end of prologue
    if((void *) header_of_block_to_free < ((void*)(sf_mem_start() + 120))){
        debug("Header is before end of prologue");
        sf_errno = EINVAL;
        return NULL;
    }

    // 4b) Footer is after begining of epilogue
    sf_footer *footer_of_block_to_free =  getFooterFromHeader(header_of_block_to_free);
    debug("Footer of the block to be freed : %p",footer_of_block_to_free);
    if((void *)footer_of_block_to_free >= (void*)(sf_mem_end()) - sizeof(sf_epilogue)){
        debug("Footer is after begining of epilogue");
        sf_errno = EINVAL;
        return NULL;
    }


   // 5) prev_all bit and alloc bit from prev_block_header must be same
    int prev_field = is_previous_block_allocated(header_of_block_to_free);
    if (prev_field == 0){
        sf_footer *prev_block_footer = (sf_footer*) ((void *)header_of_block_to_free-8);
        sf_header *prev_block_header = getHeaderFromFooter(prev_block_footer);
        debug("Size of Prev Block: %ld, Prev Header: %p , Prev Footer: %p",
            get_size(*prev_block_footer),prev_block_header,prev_block_footer);
        int is_allocated_from_prev_block_header = is_block_allocated(prev_block_header);
        if(is_allocated_from_prev_block_header!=0){
            debug("prev_alloc fields is 0 but the alloc field of the prev header is not zero");
            sf_errno = EINVAL;
            return NULL;
        }
    }
    return pp;
}

void free_current_header_and_footer(sf_header *header_of_block_to_free,sf_footer *footer_of_block_to_free){
    *header_of_block_to_free = *header_of_block_to_free & ~0x01;
    *footer_of_block_to_free = *header_of_block_to_free;
}

void change_next_block_header_and_footer(sf_footer *footer_of_block_to_free){

    sf_header *header_of_next_block_to_be_freed = (sf_header*) ((void*)(footer_of_block_to_free) + sizeof(sf_footer)) ;
    *header_of_next_block_to_be_freed = *header_of_next_block_to_be_freed & ~0x02;

    if((void *)header_of_next_block_to_be_freed == (void *)epilogue_block){
        (*epilogue_block).header =  0 | 0x01;
    }else{
        sf_footer *footer_of_next_block_to_be_freed = getFooterFromHeader(header_of_next_block_to_be_freed);
        *footer_of_next_block_to_be_freed = *header_of_next_block_to_be_freed;
    }
    return;
}

void coallesce(sf_block* current_sf_block){

    sf_header* current_block_header =  &(*current_sf_block).header;
    sf_footer* current_block_footer = getFooterFromHeader(current_block_header);
    int prev_block_allocated = is_previous_block_allocated(current_block_header)>>1;
    debug("Sf Block sent: %p",current_sf_block);
    sf_header* prev_block_header = NULL;
    if(prev_block_allocated == 0){
        sf_footer* prev_block_footer = &(*current_sf_block).prev_footer;
        prev_block_header = getHeaderFromFooter(prev_block_footer);
        debug("Prev Block Footer Location : %p",prev_block_footer);
        debug("Prev Block Header Location : %p",prev_block_header);

    }
    //TODO: What if there is no next block footer like in epilogue
    sf_header* next_block_header = (sf_header*) ((void*)(current_block_footer) + sizeof(sf_footer)) ;
    sf_header* next_block_footer = getFooterFromHeader(next_block_header);

    debug("Current Block Header Location: %p",current_block_header);
    debug("Current Block Footer Location : %p",current_block_footer);
    debug("Next Block Header Location : %p",next_block_header);

    int next_block_allocated = is_block_allocated(next_block_header);
    debug("Prev Block Allocation Status: %d\n",prev_block_allocated);
    debug("Next Block Allocation Status: %d\n",next_block_allocated);

    //check if next_block is the last block
    int is_next_block_epilogue = 0;
    if(next_block_allocated == 0){
        if(((sf_epilogue*) ((void*)(next_block_footer +1)) == epilogue_block)){
            is_next_block_epilogue = 1;
        }
    }

    //if current block is the last block
    if(((sf_epilogue*) ((void*)(current_block_footer +1)) == epilogue_block)){
            is_next_block_epilogue = 1;
        }

    if(prev_block_allocated == 1 && next_block_allocated == 1){
        size_t total_size = get_size(*current_block_header) ;
        *current_block_header =  total_size | 0x02;
        *current_block_footer = *current_block_header;
        add_to_free_circular_dll(current_sf_block,computeBlock(get_size(total_size)));
        return;
    }else if(prev_block_allocated == 1 && next_block_allocated == 0){
        size_t total_size = get_size(*current_block_header) + get_size(*next_block_header);
        *current_block_header =  total_size | 0x02;
        *next_block_footer = *current_block_header;
        remove_block_from_free_list((sf_block *)current_block_footer);
        int blockNumber = computeBlock(get_size(total_size));
        if(is_next_block_epilogue == 1){
            is_next_block_epilogue = 0;
            add_to_free_circular_dll(current_sf_block,9);
        }else{
            add_to_free_circular_dll(current_sf_block,blockNumber);
        }
        return;
    }else if(prev_block_allocated == 0 && next_block_allocated == 1){
        size_t total_size = get_size(*prev_block_header) + get_size(*current_block_header);
        int prev_prev_allocated = is_previous_block_allocated(prev_block_header);
        if(prev_prev_allocated == 1){
            *prev_block_header = total_size | 0x02;
        }else{
            *prev_block_header = total_size;
        }
        *current_block_footer = *prev_block_header;
        sf_footer* prev_prev_footer = (sf_footer*)((void*)(prev_block_header) - sizeof(sf_footer)) ;
        remove_block_from_free_list((sf_block *)prev_prev_footer);
        int blockNumber = computeBlock(get_size(total_size));
        if(is_next_block_epilogue == 1){
            is_next_block_epilogue = 0;
            add_to_free_circular_dll((sf_block *)prev_prev_footer,9);
        }else{
            add_to_free_circular_dll((sf_block *)prev_prev_footer,blockNumber);
        }
        return;
    }else{
        size_t total_size = get_size(*prev_block_header) + get_size(*current_block_header) + get_size(*next_block_header);
        int is_prev_prev_block_allocated = is_previous_block_allocated(prev_block_header);
        *prev_block_header = total_size | is_prev_prev_block_allocated;
        *next_block_footer = *prev_block_header;
        remove_block_from_free_list((sf_block *)current_block_footer);
        sf_footer* prev_prev_footer = (sf_footer*)((void*)(prev_block_header) - sizeof(sf_footer)) ;
        remove_block_from_free_list((sf_block *)prev_prev_footer);
        int blockNumber = computeBlock(get_size(total_size));
        if(is_next_block_epilogue == 1){
            is_next_block_epilogue = 0;
            add_to_free_circular_dll((sf_block *)prev_prev_footer,9);
        }else{
            add_to_free_circular_dll((sf_block *)prev_prev_footer,blockNumber);
        }
        return;
    }
}

int isAlignmentPowerOfTwo(int size){
    while( size != 1){
        if(size % 2 != 0){
            return 0;
        }
        size = size/2;
    }
    return 1;
}