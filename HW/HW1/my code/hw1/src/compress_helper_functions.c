#include "decompress_helper_functions.h"
#include "compress_helper_functions.h"

int numberOfBytesWrittenInCompress = 0;

void printHeader(unsigned int i,FILE* out){
    printInUtf8FromInt(i,out);
    return;
}

void printInUtf8FromInt(unsigned int i,FILE * out){
    if (i==0x81 || i==0x82 || i==0x83 || i==0x84 || i==0x85 ){
        fputc(i,out);
        numberOfBytesWrittenInCompress+=1;
        return;
    }else if (i>=0x0000 && i<=0x007F){
        fputc(i,out);
        numberOfBytesWrittenInCompress+=1;
        return;
    }else if(i>=0x0080&& i<=0x07FF){

        int firstNum = 0xC0 + ((i >> 6) & 0x1F);
        int secondNumber = 0x80 + (0x3F & i);

        fputc(firstNum,out);
        fputc(secondNumber,out);
        numberOfBytesWrittenInCompress+=2;
        return;
    }else if(i>=0x0800&& i<=0xFFFF){
        int firstNum = 0xE0 + ((i >> 12) & 0x0F);
        int secondNumber = 0x80 + ((i >> 6) & 0x3F);
        int thirdNumber = 0x80 + (0x3F & i);

        fputc(firstNum,out);
        fputc(secondNumber,out);
        fputc(thirdNumber,out);
        numberOfBytesWrittenInCompress+=3;
        return;
    }else{
        int firstNum = 0xF0 + ((i >> 18) & 0x07);
        int secondNumber = 0x80 + ((i >> 12) & 0x3F);
        int thirdNumber = 0x80 + ((i >> 6) & 0x3F);
        int fourthNumber = 0x80 + (0x3F & i);

        fputc(firstNum,out);
        fputc(secondNumber,out);
        fputc(thirdNumber,out);
        fputc(fourthNumber,out);
        numberOfBytesWrittenInCompress+=4;
        return;
    }
}

void formRulesAndCompressData(FILE *in, FILE *out, int bsize){
    init_symbols();
    init_rules();
    init_digram_hash();
    int count = 0;

    SYMBOL* main_rule = new_rule(next_nonterminal_value);
    add_rule(main_rule);
    next_nonterminal_value++;

    int c;
    while((c = fgetc(in)) != EOF) {
        SYMBOL* s = new_symbol(c,NULL);
        insert_after(main_rule->prev,s);
        check_digram(s->prev);
        count++;
        if(count == bsize){
            createRulesWithHeaders(out);

            init_symbols();
            init_rules();
            init_digram_hash();
            count =  0;

            debug("============================================================START_OF_BLOCK");

            SYMBOL* main_rule = new_rule(next_nonterminal_value);
            add_rule(main_rule);
            next_nonterminal_value++;
        }
    }
    if (count!=0){
        createRulesWithHeaders(out);
    }
    return ;
}

void createRulesWithHeaders(FILE* out){
    printHeader(START_OF_BLOCK,out);
    SYMBOL* rule = main_rule;
    // printf("Rule Head %d \n", rule->value);
    while(rule->nextr!=main_rule){
        // printf("Rule Head Again %d \n", rule->value);
        printRule(rule,out);
        rule = rule->nextr;
        printHeader(RULE_DELIMETER,out);
    }
    printRule(rule,out);
    printHeader(END_OF_BLOCK,out);
    return;
}

void printRule(SYMBOL* ruleHead,FILE* out){
    SYMBOL* rule = ruleHead;
    // printf("Rule Value %d \n", rule->value);
    while(rule->next != ruleHead){
        printInUtf8FromInt(rule->value,out);
        rule = rule->next;
    }
    printInUtf8FromInt(rule->value,out);
    return;
}