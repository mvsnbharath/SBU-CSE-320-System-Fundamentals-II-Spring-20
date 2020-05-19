#include <stdio.h>
#include <errno.h>
#include "sfmm.h"
#include "debug.h"
#include "custom_helper_functions.h"
#include "circular_doubly_ll_helper_functions.h"

int removed_from_free_list_block = -1;

void initialize_all_free_lists(){

    for(int i=0;i<NUM_FREE_LISTS;i++){
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
    return;
}

void add_to_free_circular_dll(struct sf_block* sf_block_to_be_inserted ,int blockNumber){
    sf_block *next_to_sentinel = sf_free_list_heads[blockNumber].body.links.next;
    sf_free_list_heads[blockNumber].body.links.next = sf_block_to_be_inserted;
    (*sf_block_to_be_inserted).body.links.prev = &sf_free_list_heads[blockNumber];
    (*sf_block_to_be_inserted).body.links.next =  next_to_sentinel;
    (*next_to_sentinel).body.links.prev =  sf_block_to_be_inserted;
    return;
}

sf_block*  get_sf_free_list_node(size_t size,int blockNumber){
    for(int i=blockNumber;i<NUM_FREE_LISTS;i++){
        sf_block* current_node = sf_free_list_heads[i].body.links.next;
        while (current_node != &sf_free_list_heads[i]){
            size_t current_node_size = get_size(current_node->header);
            if(size <= current_node_size){
                remove_block_from_free_list(current_node);
                removed_from_free_list_block = i;
                return current_node;
            }
            current_node = current_node->body.links.next;
        }
    }
    return NULL;
}

void remove_block_from_free_list(sf_block* current_node){
    sf_block* prev_block = current_node -> body.links.prev;
    sf_block* next_block = current_node -> body.links.next;

    prev_block->body.links.next = next_block;
    next_block->body.links.prev =  prev_block;
    return;
}