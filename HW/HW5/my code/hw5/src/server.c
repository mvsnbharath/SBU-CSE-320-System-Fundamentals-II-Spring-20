#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"

#include "csapp.h"
#include "server.h"
#include "pbx.h"

void *pbx_client_service(void *arg){
    //1) Read fd from arg and free
    int fd = *((int *) arg);
    free(arg);

    //2) Detach the thread
    pthread_detach(pthread_self());

    debug("[%d] Starting client service",fd);

    //3) Register the client with the given fd
    TU* tu = pbx_register(pbx,fd);
    int c;
    size_t len = 0;
    int size = 32;//initially allocate a size of 32
    FILE* fd_file = fdopen(fd,"r+");
    char* str;
    while(1){
        len = 0;
        str  = (char *) Malloc(size * sizeof(char));
        // debug("[fd: %d Extension: %d] Reading Input",tu_fileno(tu),tu_extension(tu));
        // Read input
        // debug("Checking for input for client(fd=%d)",tu_fileno(tu));
        while((c = fgetc(fd_file)) && c != '\n'){
            // debug("%d",c);
            if( feof(fd_file)) {
                debug("TU(fd=%d) received EOF, will unregister soon",fd);
                free(str);
                fclose(fd_file);
                pbx_unregister(pbx,tu);
                return NULL;
            }
            str[len++]=c;
            if(len==size){
                str = Realloc(str, sizeof(char)*(size+=32));
            }
        }

        if(len ==1){
            free(str);
            continue;
        }
        str[--len]='\0';
        debug("Command Received (%s) client(fd=%d)",str,tu_fileno(tu));
        char input_copy[strlen(str)+1];
        strcpy (input_copy, str);
        char* commandWord = strtok(input_copy," ");
        if(strcmp(commandWord,tu_command_names[TU_PICKUP_CMD])==0){
            debug("[fd: %d Extension: %d] pickup command received",tu_fileno(tu),tu_extension(tu));
            int status = tu_pickup(tu);
            if(status == -1){
                //error
                free(str);
                debug("[fd: %d Extension: %d] pickup failed",tu_fileno(tu),tu_extension(tu));
                continue;
            }
            free(str);
            continue;
        }else if(strcmp(commandWord,tu_command_names[TU_DIAL_CMD])==0){
            char* number = str+5;
            if(strlen(number) == 0){
                free(str);
                continue;
            }
            //convert char number to int
            int num = atoi(number);
            if(num <=0){
                //atoi error
                free(str);
                continue;
            }
            debug("[fd: %d Extension: %d] dial command received",tu_fileno(tu),tu_extension(tu));
            int status = tu_dial(tu,num);
            debug("[fd: %d Extension: %d] dial command status: %d",tu_fileno(tu),tu_extension(tu),status);
            if(status == -1){
                //error
                free(str);
                debug("[fd: %d Extension: %d] dial failed",tu_fileno(tu),tu_extension(tu));
                continue;
            }
            free(str);
            continue;
        }else if(strcmp(commandWord,tu_command_names[TU_HANGUP_CMD])==0){
            debug("[fd: %d Extension: %d] hangup command received",tu_fileno(tu),tu_extension(tu));
            int status = tu_hangup(tu);
            if(status == -1){
                //error
                free(str);
                debug("[fd: %d Extension: %d] hangup failed",tu_fileno(tu),tu_extension(tu));
                continue;
            }
            free(str);
            continue;
        }else if(strcmp(commandWord,tu_command_names[TU_CHAT_CMD])==0){
            char* msg = str+4;
            int status = tu_chat(tu,msg);
            debug("[fd: %d Extension: %d] chat status %d",tu_fileno(tu),tu_extension(tu),status);
            if(status == -1){
                //error
                free(str);
                debug("[fd: %d Extension: %d] chat failed",tu_fileno(tu),tu_extension(tu));
                continue;
            }
            free(str);
            continue;
        }else {
            free(str);
            continue;
        }
    }
    return NULL;
}
