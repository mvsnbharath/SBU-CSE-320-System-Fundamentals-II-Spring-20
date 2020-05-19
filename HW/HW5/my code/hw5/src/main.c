#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

int *connfd;

static void terminate(int status);

void sighup_handler(int sig){
    debug("In SIGHUP handler ");
    free(connfd);
    terminate(EXIT_SUCCESS);
}

void sigpipe_handler(int sig){
   //ignore
    debug("In SIGPIPE handler,ignoring this ");
    return;
}

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    struct sigaction action, old_action;
    action.sa_handler = sighup_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;


    if (sigaction(SIGHUP, &action, &old_action)== -1) {
        // Should not happen
        fprintf(stderr,"Error: cannot handle SIGHUP");
    }

     signal(SIGPIPE,sigpipe_handler);

    int  option;
    char *port = NULL;
    while((option = getopt(argc, argv, "p:")) != EOF) {
        switch(option) {
            case 'p':
            port = optarg;
            break;
            default:
            fprintf(stderr,"Usage: bin/pbx -p <port>\n");
            exit(EXIT_FAILURE);
        }
    }

    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");
    debug("Server Port Number %s ",port);
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.



    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);

    while(1){
        //Taken from textbook (PDF pg: 1013)
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        pthread_create(&tid, NULL, pbx_client_service, connfd);
    }

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}
