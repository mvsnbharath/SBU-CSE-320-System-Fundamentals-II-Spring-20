#include "const.h"
#include "sequitur.h"

/*
 * Digram hash table.
 *
 * Maps pairs of symbol values to first symbol of digram.
 * Uses open addressing with linear probing.
 * See, e.g. https://en.wikipedia.org/wiki/Open_addressing
 */

/**
 * Clear the digram hash table.
 */
void init_digram_hash(void) {
    //debug("Diagram Table %p",digram_table);
    // debug("Diagram Table %p",digram_tabdigram_tablele+1);
    for (int i=0;i<MAX_DIGRAMS;i++){
        *(digram_table+i)= NULL;
    }
    *digram_table = NULL;
    return;
}

/**
 * Look up a digram in the hash table.
 *
 * @param v1  The symbol value of the first symbol of the digram.
 * @param v2  The symbol value of the second symbol of the digram.
 * @return  A pointer to a matching digram (i.e. one having the same two
 * symbol values) in the hash table, if there is one, otherwise NULL.
 */
SYMBOL *digram_get(int v1, int v2) {

    debug(" Look up digram (%d, %d)",v1,v2);
    int hash = DIGRAM_HASH(v1, v2);
    int temp = hash;

    while(1){
        SYMBOL* s = *(digram_table + temp);

        if (s == TOMBSTONE){
            temp = (temp+1)%MAX_DIGRAMS;
            if(temp == hash){
                debug("Digram not found, HASH FULL");
                return NULL;
            }
            continue;
        }else if(s==NULL){
            debug("Digram not found");
            return NULL;
        }else if (s->value == v1 && s->next->value == v2){
            debug("Digram <%ld> found at index %d, add: %p",SYMBOL_INDEX(s),temp,s);
            return s;
        }
        temp = (temp+1)%MAX_DIGRAMS;

        if(temp == hash){
            debug("Digram not found, HASH FULL");
            return NULL;
        }
    }
    debug("SHOULD NEVER COME HERE");
    return NULL;
}

/**
 * Delete a specified digram from the hash table.
 *
 * @param digram  The digram to be deleted.
 * @return 0 if the digram was found and deleted, -1 if the digram did
 * not exist in the table.
 *
 * Note that deletion in an open-addressed hash table requires that a
 * special "tombstone" value be left as a replacement for the value being
 * deleted.  Tombstones are treated as vacant for the purposes of insertion,
 * but as filled for the purpose of lookups.
 *
 * Note also that this function will only delete the specific digram that is
 * passed as the argument, not some other matching digram that happens
 * to be in the table.  The reason for this is that if we were to delete
 * some other digram, then somebody would have to be responsible for
 * recycling the symbols contained in it, and we do not have the information
 * at this point that would allow us to be able to decide whether it makes
 * sense to do it here.
 */
int digram_delete(SYMBOL *digram) {
    debug("Delete digram [%ld] (%d,%d) from table ",
        SYMBOL_INDEX(digram),digram->value,digram->next->value);

    // if (SYMBOL_INDEX(digram) == 98){
    //     debug("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
    // }

    int hash = DIGRAM_HASH(digram->value,digram->next->value);
    // debug("HASH COMPUTED");
    int temp = hash;

    while(1){
        SYMBOL* s = *(digram_table + temp);
        if ( s==NULL ){
            //Entry not there
            debug("Entry not found");
            return -1;
        }else if(s == TOMBSTONE){
            temp = (temp+1)%MAX_DIGRAMS;
            //check if hash is full
            if(temp == hash){
                return -1;
            }
            continue;
        }else if (s!=NULL){
            if(s->value == digram->value && s->next->value == digram->next->value){
                //check for index also
                //Value  exists
                if(SYMBOL_INDEX(s) == SYMBOL_INDEX(digram)){
                    debug("Deleting entry at index %d",hash);
                    *(digram_table + temp)   = TOMBSTONE;
                    return 0;
                }
            }
        }
        temp = (temp+1)%MAX_DIGRAMS;
        //hash is full
        if(temp == hash){
            return -1;
        }
    }

    debug("Should never come here: Some unknown issue");
    return -1;
}

/**
 * Attempt to insert a digram into the hash table.
 *
 * @param digram  The digram to be inserted.
 * @return  0 in case the digram did not previously exist in the table and
 * insertion was successful, 1 if a matching digram already existed in the
 * table and no change was made, and -1 in case of an error, such as the hash
 * table being full or the given digram not being well-formed.
 */
int digram_put(SYMBOL *digram) {
    // debug("Delete digram :%d,%d from table ",digram->value,digram->next->value);
    // if ((IS_RULE_HEAD(digram) || IS_RULE_HEAD(digram->next))){
    //     printf("%s\n", "======YOU SIMPLY CANT====");
    //     // abort();
    // }
    debug("Add digram (%d, %d) to table",digram->value,digram->next->value);
    int hash = DIGRAM_HASH(digram->value,digram->next->value);
    int temp = hash;
    //keep track of 1st TOMBSTONE
    int firstTombstone = -1;

    while(1){
        SYMBOL* s = *(digram_table + temp);
        if ( s==NULL ){
            // we can insert here

            //check for TOMBSTONE
            if (firstTombstone==-1){
                 *(digram_table + temp)  = digram;
                debug("NULL: Adding entry at index %d, digram<%ld>",temp,SYMBOL_INDEX(digram));
                // debug("Added digram <%ld>",SYMBOL_INDEX)
                return 0;
            }else{
                *(digram_table + firstTombstone)  = digram;
                debug("TOMBSTONE: Adding entry at index %d, digram<%ld>",firstTombstone,SYMBOL_INDEX(digram));
                return 0;
            }
        }else if(s == TOMBSTONE){
            if (firstTombstone ==- 1){
                firstTombstone = temp;
            }
            temp = (temp+1)%MAX_DIGRAMS;
            //hash is full
            if(temp == hash){
                return -1;
            }
            continue;
        }else if (s!=NULL){
            if(s->value == digram->value && s->next->value == digram->next->value){
                //Value already exists
                // return 1;
                if(SYMBOL_INDEX(s) == SYMBOL_INDEX(digram)){
                    debug("Yes it exists entry at index %d",hash);
                    // *(digram_table + hash)   = TOMBSTONE;
                     return 1;
                }
            }
        }
        temp = (temp+1)%MAX_DIGRAMS;

        //hash is full
        if(temp == hash){
            return -1;
        }
    }

    debug("Should never come here: Some unknown issue");
    return -1;
}