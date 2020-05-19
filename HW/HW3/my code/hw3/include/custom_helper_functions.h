#define M 64
#define BLOCK_SIZE 64
#define ALIGNMENT 64
#define HEADER_SIZE 8
#define FOOTER_SIZE 8
#define ALLOCATED 1
#define FREE 0

extern void initialize_heap();

extern void* allocate_block_and_attach_free_block_to_the_list(sf_block* free_block,size_t total_block_size);

extern size_t compute_size_with_alignment(size_t size);

extern int computeBlock(size_t size);

extern size_t get_size(size_t size);

extern int is_previous_block_allocated(sf_header *block_header);

extern int is_block_allocated(sf_header *block_header);

extern int get_last_block_allocation_status();

extern sf_footer* getFooterFromHeader(sf_header* header);

extern sf_header* getHeaderFromFooter(sf_footer* footer);

extern sf_footer* getPrevBlockFooter(sf_header* header);

extern void validate_pointer(void *pp);

extern void* validate_realloc_pointer(void *pp);

extern void free_current_header_and_footer(sf_header *header_of_block_to_free,sf_footer *footer_of_block_to_free);

extern void change_next_block_header_and_footer(sf_footer *footer_of_block_to_free);

extern void coallesce(sf_block* free_block_sf_header);

extern int isAlignmentPowerOfTwo(int size);