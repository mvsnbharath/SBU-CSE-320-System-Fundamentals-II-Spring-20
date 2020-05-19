#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "debug.h"

#include "csapp.h"
#include "pbx.h"

typedef struct tu{
    int fd;
    int extension;
    int current_state;
    int connected_to_fd;
    int caller_fd;
    int ringing_to_fd;
    sem_t tu_mutex;
}TU;

typedef struct pbx{
 TU* tu_info[PBX_MAX_EXTENSIONS];
   int n; // total capacity
   int total_clients; //total connected items
   sem_t pbx_mutex; //access to buffer
   sem_t slots; //counts empty slots available
   sem_t items; // counts number of items present
   sem_t pbx_empty_mutex; // check if all iterms are renoved
}PBX;


PBX *pbx_init(){
    PBX* pbx = Malloc(sizeof(PBX));
    pbx->n = PBX_MAX_EXTENSIONS;
    pbx->total_clients =0;
    debug("Initializing PBX %p",pbx);

    //Functions from csapp.h
    Sem_init(&pbx->pbx_mutex,0,1); /* Binary semaphore for locking */
    Sem_init(&pbx->slots,0,pbx->n); /* Initially, buf has n empty slots */
    Sem_init(&pbx->items,0,0); /* Initially, buf has zero data items */
    Sem_init(&pbx->pbx_empty_mutex,0,1); /* Initially, buf is empty*/

    for(int i=0;i<PBX_MAX_EXTENSIONS;i++){
        pbx->tu_info[i] = NULL;
    }
    pbx->total_clients = 0;
    return pbx;
}

void pbx_shutdown(PBX *pbx){
    P(&pbx->pbx_mutex);
    for(int i=0;i<PBX_MAX_EXTENSIONS;i++){
        if(pbx->tu_info[i]!=NULL){
            debug(" Shutting down (fd=%d),",pbx->tu_info[i]->fd);
            shutdown(pbx->tu_info[i]->fd, SHUT_RDWR);
        }
    }
    V(&pbx->pbx_mutex);
    debug("Waiting for all threads to shutdown");
    P(&pbx->pbx_empty_mutex); /* Wait for all threads have deregistered */
    debug("Waiting completed for all threads to shut down");
    free(pbx);
    return;
}


TU *pbx_register(PBX *pbx, int fd){
    P(&pbx->slots); /* Wait for available slot */
    P(&pbx->pbx_mutex); /* Lock the buffer */
    TU* new_tu =malloc(sizeof(TU));
    new_tu ->fd = fd;
    new_tu->current_state = TU_ON_HOOK;
    //index where to store
    new_tu -> extension = fd % PBX_MAX_EXTENSIONS;
    new_tu -> caller_fd = -1;
    new_tu -> ringing_to_fd = -1;
    new_tu -> connected_to_fd = -1;
    Sem_init(&new_tu->tu_mutex,0,1);
    debug("Extension of this TU: %d",new_tu ->extension);
    if (pbx->tu_info[new_tu->extension] != NULL){
        //Search for the next free slot
        // If it came inside the loop there is atleast one slot available
        int k = (new_tu -> extension + 1) % PBX_MAX_EXTENSIONS;
        while(k != new_tu -> extension ){
            if(pbx->tu_info[k] == NULL){
                new_tu -> extension = k;
                debug("Extension of this TU Updated to : %d",new_tu ->extension);
                break;
            }
            k = (k+1) % PBX_MAX_EXTENSIONS;
        }
    }
    pbx->tu_info[new_tu->extension] =  new_tu;
    char str[80];
    sprintf(str, "ON HOOK %d\r\n", new_tu->extension);
    int write_output = write(fd,str,strlen(str));
    if(write_output!=strlen(str)){
        debug("Write failed while registernig  %d",write_output);
    }
    fsync(fd);
    pbx->total_clients ++;
    if(pbx->total_clients == 1){
        P(&pbx->pbx_empty_mutex);//No longer empty, do this after u have one client
    }
    debug("Registration Successful TU stored in PBX at extension : %ld",&pbx->tu_info[new_tu->extension] - pbx->tu_info);
    V(&pbx->pbx_mutex); /* Unlock the buffer */
    V(&pbx->items); /* Announce available item */
    return new_tu;
}

int get_extension_from_fd(int fd){
    for(int i =0;i<PBX_MAX_EXTENSIONS;i++){
        if(pbx->tu_info[i]!=NULL && pbx->tu_info[i]->fd == fd){
            return pbx->tu_info[i]->extension;
        }
    }
    return -1;
}

int get_fd_from_extension(int ext){
    for(int i =0;i<PBX_MAX_EXTENSIONS;i++){
        if(pbx->tu_info[i]!=NULL && pbx->tu_info[i]->extension == ext){
            return pbx->tu_info[i]->fd;
        }
    }
    return -1;
}

void check_status_and_perform_other_actions(TU* tu){
   P(&tu->tu_mutex);
   if(tu->current_state == TU_CONNECTED){
    //check to whoom it's connected
     int peer_fd = tu->connected_to_fd;
     TU* peer_tu = pbx->tu_info[get_extension_from_fd(peer_fd)];
     P(&peer_tu->tu_mutex);
     peer_tu->current_state = TU_DIAL_TONE;
     peer_tu->connected_to_fd = -1;
     peer_tu->caller_fd = -1;
     peer_tu->ringing_to_fd = -1;
     char str[80];
     sprintf(str, "DIAL TONE\r\n");
     int write_output =write(peer_tu->fd,str,strlen(str));
     if(write_output!=strlen(str)){
        debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output,strlen(str));
        fsync(peer_tu->fd);
        V(&peer_tu->tu_mutex);
        V(&tu->tu_mutex);
        return;
    }
    fsync(peer_tu->fd);
    V(&peer_tu->tu_mutex);
    V(&tu->tu_mutex);
    return;
}else if(tu->current_state == TU_RING_BACK){
    //change ringing to ON HOOK
 int ringing_fd = tu->ringing_to_fd;;
 TU* ringing_tu = pbx->tu_info[get_extension_from_fd(ringing_fd)];
 P(&ringing_tu->tu_mutex);

   //Ringing TU
 ringing_tu->current_state = TU_ON_HOOK;
 ringing_tu->connected_to_fd = -1;
 ringing_tu->caller_fd = -1;
 ringing_tu->ringing_to_fd = -1;
 char str2[80];
 sprintf(str2, "ON HOOK %d\r\n",ringing_tu->extension);
 int write_output2 =write(ringing_tu->fd,str2,strlen(str2));
 if(write_output2!=strlen(str2)){
    debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output2,strlen(str2));
    fsync(ringing_tu->fd);
    V(&ringing_tu->tu_mutex);
    V(&tu->tu_mutex);
    return;
}
fsync(ringing_tu->fd);
V(&ringing_tu->tu_mutex);
V(&tu->tu_mutex);
return;
}else if(tu->current_state == TU_RINGING){

    int called_fd = tu->caller_fd;
    TU* called_tu = pbx->tu_info[get_extension_from_fd(called_fd)];
    P(&called_tu->tu_mutex);

    //Called TU
    called_tu->current_state = TU_DIAL_TONE;
    called_tu->connected_to_fd = -1;
    called_tu->caller_fd = -1;
    called_tu->ringing_to_fd = -1;
    char str2[80];
    sprintf(str2, "DIAL TONE\r\n");
    int write_output2 = write(called_tu->fd,str2,strlen(str2));
    if(write_output2!=strlen(str2)){
        debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output2,strlen(str2));
        fsync(called_tu->fd);
        V(&called_tu->tu_mutex);
        V(&tu->tu_mutex);
        return;
    }
    fsync(called_tu->fd);
    V(&called_tu->tu_mutex);
    V(&tu->tu_mutex);
    return;
}
}

int pbx_unregister(PBX *pbx, TU *tu){
    P(&pbx->items); /* Wait for available item */
    P(&pbx->pbx_mutex); /* Lock the buffer */
    debug("De-Registration Initiated for TU (extension = %d) ",tu->fd);
    int extension = tu->extension;
    check_status_and_perform_other_actions(tu);
    free(tu);
    pbx->tu_info[extension] = NULL;
    debug("De-Registration Completed for the TU");
    pbx->total_clients--;
    debug("Total clients %d",pbx->total_clients);
    V(&pbx->pbx_mutex);/* Unlock the buffer */
    V(&pbx->slots); /* Announce available slot */
    if(pbx->total_clients == 0){
        V(&pbx->pbx_empty_mutex); /* Announce all threads have deregistered */
    }
    return 0;
}

int tu_fileno(TU *tu){
    if(tu == NULL){
        return -1;
    }
    return tu->fd;;
}

int tu_extension(TU *tu){
    if(tu == NULL){
        return -1;
    }
    return tu->extension;
}

void printCurrentState(TU* tu){
    int current_state = tu->current_state;
    char str[80];
    if(current_state == TU_ON_HOOK){
        sprintf(str, "ON HOOK %d\r\n",tu->extension);
    }else if(current_state == TU_RINGING){
        sprintf(str, "RINGING\r\n");
    }else if(current_state == TU_DIAL_TONE){
        sprintf(str, "DIAL TONE\r\n");
    }else if(current_state == TU_RING_BACK){
        sprintf(str, "RING BACK\r\n");
    }else if(current_state == TU_BUSY_SIGNAL){
        sprintf(str, "BUSY SIGNAL\r\n");
    }else if(current_state == TU_CONNECTED){
        int peer_ext = get_extension_from_fd(tu->connected_to_fd);
        sprintf(str, "CONNECTED %d\r\n",peer_ext);
    }else if(current_state == TU_ERROR){
        sprintf(str, "ERROR\r\n");
    }
    int write_output = write(tu->fd,str,strlen(str));
    if(write_output!=strlen(str)){
        debug("Write failed (expected = %d, got= %d)",strlen(str),write_output);
    }
    fsync(tu->fd);
    return;
}


int tu_pickup(TU *tu){
    P(&tu->tu_mutex);
    if(tu->current_state == TU_ON_HOOK){
        // P(&tu->tu_mutex);
        tu->current_state = TU_DIAL_TONE;
        char str[80];
        sprintf(str, "DIAL TONE\r\n");
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while pickup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);
        V(&tu->tu_mutex);
        debug("Pickup Successful for TU(ext=%d), changed to 'DIAL TONE' ",tu->extension);
        return 0;
    }else if(tu->current_state == TU_RINGING){
        debug("TU Caller fd %d ",tu->caller_fd);

        //Peer
        int caller_fd = tu->caller_fd;
        int caller_ext = get_extension_from_fd(caller_fd);
        TU* caller_tu = pbx->tu_info[caller_ext];
        P(&caller_tu->tu_mutex);

        tu->current_state = TU_CONNECTED;
        tu->connected_to_fd = tu->caller_fd;
        char str[80];
        int peer_ext = get_extension_from_fd(tu->connected_to_fd);

        sprintf(str, "CONNECTED %d\r\n",peer_ext);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while pickup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&caller_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);
        caller_tu->current_state = TU_CONNECTED;
        caller_tu ->connected_to_fd = tu->fd;
        char str2[80];
        sprintf(str2, "CONNECTED %d\r\n",tu->extension);
        write_output = write(caller_fd,str2,strlen(str2));
        if(write_output!=strlen(str2)){
            debug("Write failed while pickup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(caller_tu->fd);
            V(&caller_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(caller_tu->fd);
        V(&caller_tu->tu_mutex);
        debug("Pickup Successful for TU's(ext= %d, %d), changed to 'CONNECTED' ",
            tu->extension,caller_ext);
        V(&tu->tu_mutex);
        return 0;

    }else{
        //No state change
        printCurrentState(tu);
        V(&tu->tu_mutex);
        return 0;
    }
    return -1;
}

int serach_for_extension(int ext){
    for (int i=0;i<PBX_MAX_EXTENSIONS;i++){
        if(pbx->tu_info[i]!=NULL && pbx->tu_info[i]->extension == ext){
            return ext;
        }
    }
    return -1;
}


int tu_dial(TU *tu, int ext){

    P(&tu->tu_mutex);
    if(tu->current_state == TU_DIAL_TONE){

        //serach for the extension;
        int index = serach_for_extension(ext);
        if(index == -1){
            //failure, change it to TU_ERROR
            tu->current_state = TU_ERROR;
            char str[80];
            sprintf(str, "ERROR\r\n");
            int write_output = write(tu->fd,str,strlen(str));
            if(write_output!=strlen(str)){
                debug("Write failed while dial  %d",write_output);
                fsync(tu->fd);
                V(&tu->tu_mutex);
                return -1;
            }
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return 0;
        }

        //check for called state
        TU* tu_callee = pbx->tu_info[ext];
        if(tu_callee->current_state == TU_ON_HOOK){
            //go to ringing
            //tu goes to ringback
            P(&tu_callee->tu_mutex);
            tu->current_state = TU_RING_BACK;
            int callee_fd = get_fd_from_extension(ext);
            tu-> ringing_to_fd = callee_fd;

            char str[80];
            sprintf(str, "RING BACK\r\n");
            int write_output = write(tu->fd,str,strlen(str));
            if(write_output!=strlen(str)){
                debug("Write failed while dial (got = %d, exptectd = %ld)",write_output,strlen(str));
                fsync(tu->fd);
                V(&tu_callee->tu_mutex);
                V(&tu->tu_mutex);
                return -1;
            }
            fsync(tu->fd);

            tu_callee->current_state = TU_RINGING;
            tu_callee ->caller_fd = tu->fd;
            char str2[80];
            sprintf(str2, "RINGING\r\n");
            write_output = write(tu_callee->fd,str2,strlen(str2));
            if(write_output!=strlen(str2)){
                debug("Write failed while dial (got = %d, exptectd = %ld)",write_output,strlen(str2));
                fsync(tu_callee->fd);
                V(&tu_callee->tu_mutex);
                V(&tu->tu_mutex);
                return -1;
            }
            fsync(tu_callee->fd);
            debug("Dial Successful for TU's(ext= %d, %d)",tu->extension,tu_callee->extension);
            V(&tu_callee->tu_mutex);
            V(&tu->tu_mutex);
            return 0;
        }else{
            //busy state
            tu->current_state = TU_BUSY_SIGNAL;
            char str[80];
            sprintf(str, "BUSY SIGNAL\r\n");
            int write_output = write(tu->fd,str,strlen(str));
            if(write_output!=strlen(str)){
                debug("Write failed while dial  %d",write_output);
                fsync(tu->fd);
                V(&tu->tu_mutex);
                return -1;
            }
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return 0;
        }
    }else{
        //No change in state
        printCurrentState(tu);
        V(&tu->tu_mutex);
        return 0;
    }
    return -1;
}

int tu_hangup(TU *tu){
    P(&tu->tu_mutex);
    if(tu->current_state == TU_CONNECTED){
        //Transisition the peer connected state to TU_DIAL_TONE
        int peer_fd = tu->connected_to_fd;
        debug("Hangup Case I");
        TU* peer_tu = pbx->tu_info[get_extension_from_fd(peer_fd)];
        P(&peer_tu->tu_mutex);

        //TU
        tu->current_state = TU_ON_HOOK;
        tu->connected_to_fd = -1;
        tu->caller_fd = -1;
        tu->ringing_to_fd = -1;
        char str[80];
        sprintf(str, "ON HOOK %d\r\n", tu->extension);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);

        //Peer TU
        peer_tu->current_state = TU_DIAL_TONE;
        peer_tu->connected_to_fd = -1;
        peer_tu->caller_fd = -1;
        peer_tu->ringing_to_fd = -1;
        char str2[80];
        sprintf(str2, "DIAL TONE\r\n");
        int write_output2 = write(peer_tu->fd,str2,strlen(str2));
        if(write_output2!=strlen(str2)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output2,strlen(str2));
            fsync(peer_tu->fd);
            V(&peer_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(peer_tu->fd);

        V(&peer_tu->tu_mutex);
        V(&tu->tu_mutex);
        return 0;

    }else if(tu->current_state == TU_RING_BACK){
        int ringing_fd = tu->ringing_to_fd;;

        debug("Hangup Case II");
        TU* ringing_tu = pbx->tu_info[get_extension_from_fd(ringing_fd)];
        P(&ringing_tu->tu_mutex);

        //TU
        tu->current_state = TU_ON_HOOK;
        tu->connected_to_fd = -1;
        tu->caller_fd = -1;
        tu->ringing_to_fd = -1;
        char str[80];
        sprintf(str, "ON HOOK %d\r\n", tu->extension);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);

        //Ringing TU
        ringing_tu->current_state = TU_ON_HOOK;
        ringing_tu->connected_to_fd = -1;
        ringing_tu->caller_fd = -1;
        ringing_tu->ringing_to_fd = -1;
        char str2[80];
        sprintf(str2, "ON HOOK %d\r\n",ringing_tu->extension);
        int write_output2 = write(ringing_tu->fd,str2,strlen(str2));
        if(write_output2!=strlen(str2)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output2,strlen(str2));
            fsync(ringing_tu->fd);
            V(&ringing_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(ringing_tu->fd);

        V(&ringing_tu->tu_mutex);
        V(&tu->tu_mutex);
        return 0;

    }else if(tu->current_state == TU_RINGING){
        int called_fd = tu->caller_fd;

        debug("Hangup Case III");
        // P(&tu->tu_mutex);
        TU* called_tu = pbx->tu_info[get_extension_from_fd(called_fd)];
        P(&called_tu->tu_mutex);

        //TU
        tu->current_state = TU_ON_HOOK;
        tu->connected_to_fd = -1;
        tu->caller_fd = -1;
        tu->ringing_to_fd = -1;
        char str[80];
        sprintf(str, "ON HOOK %d\r\n", tu->extension);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);

        //Called TU
        called_tu->current_state = TU_DIAL_TONE;
        called_tu->connected_to_fd = -1;
        called_tu->caller_fd = -1;
        called_tu->ringing_to_fd = -1;
        char str2[80];
        sprintf(str2, "DIAL TONE\r\n");
        int write_output2 = write(called_tu->fd,str2,strlen(str2));
        if(write_output2!=strlen(str2)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output2,strlen(str2));
            fsync(called_tu->fd);
            V(&called_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(called_tu->fd);

        V(&called_tu->tu_mutex);
        V(&tu->tu_mutex);
        return 0;

    }else if(tu->current_state == TU_DIAL_TONE ||
        (tu->current_state == TU_BUSY_SIGNAL || tu->current_state == TU_ERROR)){

        //TU
        debug("Hangup Case IV");
        tu->current_state = TU_ON_HOOK;
        tu->connected_to_fd = -1;
        tu->caller_fd = -1;
        tu->ringing_to_fd = -1;
        char str[80];
        sprintf(str, "ON HOOK %d\r\n", tu->extension);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while hangup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);
        V(&tu->tu_mutex);
        return 0;
    }else if(tu->current_state == TU_ON_HOOK ){
        debug("Hangup Case V");
        tu->connected_to_fd = -1;
        tu->caller_fd = -1;
        tu->ringing_to_fd = -1;
        printCurrentState(tu);
        V(&tu->tu_mutex);
        return 0;
    }
    return -1;
}

int tu_chat(TU *tu, char *msg){
    P(&tu->tu_mutex);
    if(tu->current_state == TU_CONNECTED){
        int peer_fd = tu->connected_to_fd;
        TU* peer_tu = pbx->tu_info[get_extension_from_fd(peer_fd)];
        P(&peer_tu->tu_mutex);
        debug("Message length: %ld",strlen(msg));
        //Peer TU
        char str2[10+strlen(msg)];
        if (strlen(msg) == 0){
            //if len(msg) is 0 it will have \0 and \r\n might not be sent
            sprintf(str2, "CHAT\r\n");
        }else{
            sprintf(str2, "CHAT%s\r\n",msg);
        }
        int write_output2 = write(peer_tu->fd,str2,strlen(str2));

        if(write_output2!=strlen(str2)){
            debug("Write failed while chat (got = %d, exptectd = %ld)",write_output2,strlen(str2));
            fsync(peer_tu->fd);
            V(&peer_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(peer_tu->fd);

        char str[80];
        int peer_ext = get_extension_from_fd(tu->connected_to_fd);
        sprintf(str, "CONNECTED %d\r\n",peer_ext);
        int write_output = write(tu->fd,str,strlen(str));
        if(write_output!=strlen(str)){
            debug("Write failed while pickup (got = %d, exptectd = %ld)",write_output,strlen(str));
            fsync(tu->fd);
            V(&peer_tu->tu_mutex);
            V(&tu->tu_mutex);
            return -1;
        }
        fsync(tu->fd);
        V(&peer_tu->tu_mutex);
        V(&tu->tu_mutex);
        debug("Exiting from CHAT");
        return 0;

    }else{
        //In all other cases
        printCurrentState(tu);
        V(&tu->tu_mutex);
        return -1;
    }
    return -1;
}