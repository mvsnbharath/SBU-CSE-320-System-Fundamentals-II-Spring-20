#include "const.h"
#include "debug.h"
#include "validate_args_helper_functions.h"

int compareStrings(char *string_x, char  *string_y){

   while((*string_x!='\0' || *string_y!='\0')  && (*string_x ==*string_y)){
    //Condition to check if characters are same and check for termination in either of the Strings
       string_x++;
       string_y++;
   }

   if(*string_x=='\0' && *string_y=='\0'){
       return 0;
   }
   return -1;
}

int validateArgsAfterC(char **argv, int currentPosition, int argLength){
    //valid args after c
    //-c (or)  -c -b 1024
    // possible values for argLength 2  or 4

    if(argLength ==2){
        //default block size 1024
        global_options = 1024<<16;
        return 0;
    }else if(argLength == 4){
        if(compareStrings(*(argv+currentPosition+1),"-b")==0){
         block_size =  computeBlockSize(*(argv+currentPosition+2));
         debug("Block Size %d",block_size);
         if (block_size==-1)
           return-1;
       global_options = block_size<<16;
       return 0;
   }
}
return -1;
}

int validateArgsAfterD(char **argv, int currentPosition, int argLength){

    if(compareStrings(*(argv+currentPosition+1),"-c")==0){
        return 0;
    }else{
        return -1;
    }
}

int computeBlockSize (char *block_size_string){
    debug("Block Size as string: %s",block_size_string);
    int block_size = 0;

    while((*block_size_string!='\0') ){
        if (*block_size_string<'0' || *block_size_string >'9')
            return -1;

        block_size = (10*block_size) + (*block_size_string-'0');

        if (block_size<=0 || block_size>1024)
            return -1;

        block_size_string++;
    }

    return block_size;
}