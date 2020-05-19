extern void initialize_all_free_lists();

extern void add_to_free_circular_dll(struct sf_block* sf_block_to_be_inserted ,int blockNumber);

extern sf_block* get_sf_free_list_node(size_t size,int blockNumber);

extern void remove_block_from_free_list(sf_block* current_node);