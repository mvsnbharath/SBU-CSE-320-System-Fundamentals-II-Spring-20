#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "debug.h"
#include "polya.h"

sig_atomic_t sighup_flag;

void sighup_handler(int sig){
    sighup_flag =1 ;
    debug("[%ld:Worker] Received Signal %d", (long)getpid(),sig);
    debug("[%ld:Worker]SIGHUP received -- canceling current solution attempt", (long)getpid());
}

void sigterm_handler(int sig){
    //Graceful exit
    debug("[%ld:Worker] Received Signal %d", (long)getpid(),sig);
    fclose(stdin);
    fclose(stdout);
    exit(0);
}

void sigpipe_handler(int sig){
    debug("[%ld:Master] Ignorong SIGPIPE handler, Received signal %d", (long)getpid(),sig);
    return;
}

/*
 * worker
 * (See polya.h for specification.)
 */
int worker(void) {
    //Initialization
    signal(SIGHUP, sighup_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGPIPE, sigpipe_handler);

    debug("[%ld:Worker] Starting", (long)getpid());
    //Let Parent Know you are idle
    debug("[%ld:Worker] Idling(Sending SIGSTOP to self)", (long)getpid());
    kill(getpid(),SIGSTOP);
    struct problem *p = NULL;
    struct result *r = NULL;

    while(1){
        //Read Header
        sighup_flag = 0;
        p = NULL;
        r = NULL;
        p = (struct problem *) malloc(sizeof(struct problem));
        int count = fread(p, sizeof(char),sizeof(struct problem), stdin);
        debug("[%ld:Worker] Reading Problem", (long)getpid());
        if (count != sizeof(struct problem)){
            debug("[%ld: Worker] UNEXPECTED BEHAVIOUR while reading header (read %d, expected %ld)",
             (long)getpid(),count, sizeof(struct problem));
        }
        debug("[%ld:Worker] Read data (fd = %d, nbytes = %ld)", (long)getpid(),0,p->size);
        // debug("[%ld:Worker] Size %ld Type %d , id %d, nvars %d var %d",
        //     (long)getpid(),p->size, p->type, p->id,p->nvars, p->var);
        size_t payload_size = p->size - sizeof(struct problem);
        // debug("[%ld:Worker] Payload Size %ld", (long)getpid(),payload_size);
        p = realloc(p,p->size);
        void* payload_begin = (void *)(p)+sizeof(struct problem);
        // debug("[%ld:Worker] Ponter P %p", (long)getpid(),p);
        // debug("[%ld:Worker] Ponter P+16 %p", (long)getpid(),payload_begin);
        count = fread(payload_begin, sizeof(char), payload_size , stdin);
        if (count != payload_size) {
            debug("[%ld:Worker] UNEXPECTED BEHAVIOUR while reading problem, (read %d, expected %ld)",
                (long)getpid(),count,payload_size );
        }
        debug("[%ld:Worker] Got problem size %ld ,type %d ,variants %d ",
            (long)getpid(),p->size, p->type,p->nvars);

        // Solving To Start Problem
        debug("[%ld:Worker] Solving Problem",(long)getpid());
        r = solvers[p->type].solve(p,&sighup_flag);

        if(r == NULL){
            //Worker Failure Case
            debug("[%ld:Worker] Solver canceled",(long)getpid());
            r = (struct result *) malloc(sizeof(struct result));
            r->size = sizeof(struct result);
            r-> id = p->id;
            r-> failed = 1;
        }else if(r->failed == 0){
            //Worker Success Case
            debug("[%ld:Worker] Solver returned a result",(long)getpid());
        }

        debug("[%ld:Worker] Writing result: size = %ld, failed = %d",
            (long)getpid(),r->size,r->failed);
        debug("[%ld:Worker] Write data (fd = %d, nbytes = %ld)", (long)getpid(),1,sizeof(struct result));

        //Write heder (for both success and failure)
        int written =fwrite(r, sizeof(char) ,sizeof(struct result),stdout);
        if (written != sizeof(struct result)){
            debug("[%ld: Worker] UNEXPECTED BEHAVIOUR while writing header", (long)getpid());
        }


        //Write data only in success case
        if(r->failed == 0){
            size_t result_size = r->size - sizeof(struct result);
            void* result_begin = (void *)(r)+sizeof(struct result);
            // debug("[%ld:Worker] Result R %p", (long)getpid(),r);
            // debug("[%ld:Worker] Result R+16 %p", (long)getpid(),result_begin);
            written = fwrite(result_begin, sizeof(char), result_size , stdout);
            if (written != result_size) {
                debug("[%ld: Worker] UNEXPECTED BEHAVIOUR while reading problem", (long)getpid());
            }
        }
        debug("[%ld: Worker] Flush stdout", (long)getpid());
        fflush(stdout);
        // Free both problem and result
        // free(r);
        // free(p);

        //Indicating parent to read
        debug("[%ld: Worker] Idling(Sending SIGSTOP to self)", (long)getpid());
        kill(getpid(),SIGSTOP);
    }
    return EXIT_SUCCESS;
}
