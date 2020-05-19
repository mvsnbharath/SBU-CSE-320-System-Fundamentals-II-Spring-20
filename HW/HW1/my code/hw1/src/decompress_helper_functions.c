#include "decompress_helper_functions.h"

int numberOfBytesWritten = 0;

int flag = 0;

int validateHeader(int fieldType,FILE *in){
    //check magicBytes
    if(fgetc(in)!=fieldType){
        return -1;
    }
    debug("Header Validated: 0x%02x",fieldType);
    return 0;
}


int decompressBlock(FILE *in,FILE*out){

    int  fieldType;
    while((fieldType=fgetc(in))){
     if(fieldType==START_OF_BLOCK){
        //if SOB is reached flag should be 0
        if (flag == 0){
            debug("START_OF_BLOCK");

            int decompress_rule = decompressRule(in);
            if (decompress_rule == END_OF_BLOCK){
                //We should receive EOB with flag 1
                debug("END_OF_BLOCK");
                if(flag !=0){
                    return -1;
                }
                expandRule(main_rule,out);
                init_symbols();
                init_rules();
                continue;
            }else{
                debug("Reached here %d",decompress_rule);
                return -1;
            }
        }else{
            //SOB and flag is not 0
            return -1;
        }
    }else if(fieldType == END_OF_TRANSMISSION){
        //When program reaches EOT, flag should be 0
        if (flag == 0){
            fflush(out);
            return 0;
        }else{
            //EOT and flag is not zero
            return -1;
        }
    }else{
            //error
        return -1;
    }
}
return -1;
}

int decompressRule(FILE* in){
    //debug("NEW_RULE");
    flag = 1;
    int i = fgetc(in);
    int value = get_utf8(in,i);
    //Non-Terminal:
    // debug("Rule Head Value: 0x%02x",value);
    if (value>=FIRST_NONTERMINAL){
        //New Rule (Head)
        SYMBOL* newRule = new_rule(value);
        //add this rule to the existing rule
        add_rule(newRule);

         //Form Rule
        while((i=get_utf8(in,fgetc(in)))){
            //NO SOT,EOT,SOB
            if(i == START_OF_TRANSMISSION || i == END_OF_TRANSMISSION || i==START_OF_BLOCK){
                return -1;
            }
            if (i == RULE_DELIMETER){
                //debug("RULE_DELIMETER");
                //New Rule
                if(decompressRule(in)==-1){
                    return -1;
                }
                flag = 0;
                return END_OF_BLOCK;

            }else if(i == END_OF_BLOCK){
                flag = 0;
                return END_OF_BLOCK;
            }else{
                //Add Symbols to the rule
                SYMBOL* newSymbol = addSymbol(i);
                addSymbolToTheRule(newSymbol,newRule);
            }
        }
    }else{
        //Rule not starting with Non-terminal character
        debug("Invalid Start for a Rule: 0x%02x",value);
        return -1;
    }
    //Should never reach here
    debug("Should Never Reach here");
    return -1;
}

SYMBOL* addSymbol(int value){
    return new_symbol(value,NULL);
}

void addSymbolToTheRule(SYMBOL* newSymbol, SYMBOL* ruleHead){
    (*ruleHead).prev->next = newSymbol;
    (*newSymbol).prev = (*ruleHead).prev;
    (*newSymbol).next = ruleHead;
    (*ruleHead).prev = newSymbol;
    return;
}

int get_utf8(FILE * in, int i){
    if (i==RULE_DELIMETER){
        // debug("RD from utf8");
        return RULE_DELIMETER;
    }else if(i == END_OF_BLOCK){
        //debug("RD from EOB");
        return END_OF_BLOCK;
    }else if ((i & 0xF0) ==0xF0){
        // debug("4 bytes");
        return  ((i&7)<<18) + ((fgetc(in)&0x3F)<<12) + ((fgetc(in)&0x3F)<<6) +(fgetc(in)&0x3F);
    } else if((i & 0xE0) == 0xE0) {
        int k = ((i&0xF)<<12) + ((fgetc(in)&0x3F)<<6) +(fgetc(in)&0x3F);
        // debug("3 bytes: 0x%02x",k);
        return k;
        // return  ((i&0xF)<12) + ((fgetc(in)&0x3F)<6) +(fgetc(in)&0x3F);
    }else if((i & 0xC0) == 0xC0){
        // int k = ((i&0x1F)<<6) + (fgetc(in)&0x3F);
        // debug("2 bytes: 0x%02x",k);
        // return k;
        return ((i&0x1F)<<6) + (fgetc(in)&0x3F);
    }else if(((i>>7)&1)== 0){
         // debug("1 byte");
        return i&0x7F;
    }else{
        // debug("not here");
        return -1;
    }
}

void expandRule(SYMBOL* rule,FILE* out){
    SYMBOL* rule_head = rule;
    while(rule->next!=rule_head){
        if ((rule->next->value >= FIRST_NONTERMINAL)){
            SYMBOL* nextRule = rule->next;
            expandRule(*(rule_map+(nextRule->value)),out);
        }else{
            fputc(rule->next->value,out);
            numberOfBytesWritten+=1;
        }
        rule = rule->next;
    }
}