#include "const.h"
#include "sequitur.h"
#include "debug.h"
#include "validate_args_helper_functions.h"
#include "decompress_helper_functions.h"
#include "compress_helper_functions.h"
#include "custom_constants.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

extern int numberOfBytesWritten;
extern int numberOfBytesWrittenInCompress;

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    printHeader(START_OF_TRANSMISSION,out);
    formRulesAndCompressData(in,out,bsize);
    printHeader(END_OF_TRANSMISSION,out);
    return numberOfBytesWrittenInCompress;
}

/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {

    // Initialize rules.
    init_symbols();
    init_rules();

    if(validateHeader(START_OF_TRANSMISSION,in)==-1){
        return EOF;
    }

    int formRuleMap =  decompressBlock(in,out);
    if (formRuleMap != 0){
        return EOF;
    }
    debug("Number of Bytes written: %d",numberOfBytesWritten);
    return numberOfBytesWritten;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    if(argc == 1){
        // No argumets were given, argc=1
        // Default argc=1  (default value is program's name)
        return -1;
    }
    //check if 2nd argument is(-h)
    if(compareStrings(*(argv+1), "-h")==0){
        global_options =1;
        return 0;
    }

    if(argc>4){
        // Max of 4 args (1 + 3)
        // Max 3 would be (-c -b  1024)
        return -1;
    }

    //check if 2nd argument is(-c)
    if(compareStrings(*(argv+1),"-c")==0){
        //check if next argument is present or not,
        //and if present is it "-b" followed by -c
        if(validateArgsAfterC(argv,1,argc)==0){
            global_options|=2;
            return 0;
        }
    //check if 2nd argument is(-d)
    }else if(compareStrings(*(argv+1),"-d")==0 && argc==2){
        //decompress the data
     global_options = 4;
     return 0;
 }
 return -1;
}
