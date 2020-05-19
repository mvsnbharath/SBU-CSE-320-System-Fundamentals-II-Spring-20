#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#include "debug.h"
#include "polya.h"

#define READ_END 0
#define WRITE_END 1

struct worker_info{
    int status;
    pid_t pid;
    int master_read_fd;
    int master_write_fd;
    int problem_id;
    int problem_status;
    struct problem* problem_location;
};

sig_atomic_t sigchild_flag;
sig_atomic_t sigpipe_flag;

struct worker_info* worker_info = NULL;

int workers_alive;
int workers_stopped ;
int workers_idle;
int total_workers;

int atleast_one_worker_had_issues;
int no_more_problems ;

int get_index_from_pid(int child_pid){
    int i =0;
    for(i=0;i<total_workers;i++){
        if( (worker_info+i)->pid == child_pid){
            return i;
        }
    }
    return -1;
}

void perform_action_for_sig_child(int i){
    int worker_status = (worker_info+i)->status;

    if( worker_status == 1){
        //Move  Started to idle
        (worker_info+i)->status = 2;
        sf_change_state( (worker_info + i)->pid,1,2 );
        debug("[%ld:Master] Set state of worker %d (pid = %d): started->idle",
            (long)getpid(),i,(worker_info+i)->pid);
        workers_idle++;
        debug("[%ld:Master] Workers idle %d", (long)getpid(),workers_idle);

        //Check for new problem
        struct problem* p= get_problem_variant(total_workers, i);
        //Zero-problems case
        if(p == NULL){
            no_more_problems = 1;
            if (no_more_problems ==1 && total_workers == workers_idle){
                for (int j=0; j<total_workers;j++){
                    if((worker_info +j)->status == 2){
                        kill((worker_info+j)->pid,SIGTERM);
                        kill((worker_info+j)->pid,SIGCONT);
                    }else{
                        debug("[%ld:Master] Worker %d is expected to be in idle(state =2 ) but its in state %d",
                            (long)getpid(),j,(worker_info+j)->status);
                    }
                }
                debug("[%ld:Master] All live workers are idle -- terminating",(long)getpid());
                return;
            }else{
                return;
            }
        }

        (worker_info+i)->problem_location = p ;
        (worker_info+i)-> problem_status = 0;
        (worker_info+i)-> problem_id = p->id;

        debug("[%ld:Master] Assigning problem %p worker %d (pid = %d)",
            (long)getpid(),p,i,(worker_info + i)->pid);

        //Send SIGCONT
        kill((worker_info + i)-> pid,SIGCONT);
        debug("[%ld:Master] Sending SIGCONT to worker %d", (long)getpid(),i);
        //Move idle to continued
        (worker_info+i)->status = 3;
        sf_change_state( (worker_info + i)->pid,2,3 );
        debug("[%ld:Master] Set state of worker %d (pid = %d): idle->continued",
            (long)getpid(),i,(worker_info+i)->pid);
        workers_idle--;
        return;
    }
    else if ( worker_status == 3){
        if(no_more_problems == 1 ){
            //move this to idle state
            debug("[%ld:Master] Worker (pid = %d, state = %d) idled",
                (long)getpid(),(worker_info+i)->pid, (worker_info+i)->status);
            //Move continued to idle
            sf_change_state( (worker_info + i)->pid,3,2 );
            (worker_info+i)->status = 2;
            workers_idle++;
            kill((worker_info+i)->pid,SIGSTOP);

            debug("[%ld:Master] Workers idle %d",(long)getpid(),workers_idle);
            if(workers_idle == total_workers){
                for (int j=0; j<total_workers;j++){
                    if((worker_info +j)->status == 2){
                        kill((worker_info+j)->pid,SIGTERM);
                        kill((worker_info+j)->pid,SIGCONT);
                        sf_change_state((worker_info+j)->pid,2,4);
                        (worker_info +j)->status  = 4;
                        debug("[%ld:Master] Set state of worker %d (pid = %d): idle->running",
                            (long)getpid(),i,(worker_info+i)->pid);
                    }else{
                        debug("[%ld:Master] Worker %d is expected to be in idle(state =2 ) but its in state %d",
                            (long)getpid(),j,(worker_info+j)->status);
                    }
                }
                return ;
            }else{
                return;
            }
        }

        struct problem* p = (worker_info+i)->problem_location;
        if(p->size == 0){
           //move this to idle state
            debug("[%ld:Master] Worker (pid = %d, state = %d) idled",
                (long)getpid(),(worker_info+i)->pid, (worker_info+i)->status);
            //Move continued to idle
            sf_change_state( (worker_info + i)->pid,3,2 );
            (worker_info+i)->status = 2;
            workers_idle++;
            kill((worker_info+i)->pid,SIGSTOP);
            return;
        }

        debug("[%ld:Master] Worker (pid = %d, state = %d) has continued",
            (long)getpid(),(worker_info+i)->pid, (worker_info+i)->status);
        //Move continued to running
        (worker_info+i)->status = 4;
        sf_change_state( (worker_info + i)->pid,3,4 );
        debug("[%ld:Master] Set state of worker %d (pid = %d): continued->running",
            (long)getpid(),i,(worker_info+i)->pid);
        sf_send_problem((worker_info+i)->pid,p);
        int master_write_fd = (worker_info + i)->master_write_fd;
        FILE* fd = fdopen(master_write_fd, "w");
        int written =fwrite(p, sizeof(char) ,sizeof(struct problem),fd);
        if (written != sizeof(struct problem)){
            debug("[%ld: Master] UNEXPECTED BEHAVIOUR while writing header", (long)getpid());
            debug("[%ld: Master] Exiting the program", (long)getpid());
            exit(EXIT_FAILURE);
        }
        debug("[%ld:Master] Write problem (fd = %d, size = %ld)",
            (long)getpid(),master_write_fd,p->size);
        size_t problem_size = p->size - sizeof(struct problem);
        void* problem_begin = (void *)(p)+sizeof(struct problem);
        written = fwrite(problem_begin, sizeof(char), problem_size ,fd);
        if (written != problem_size) {
            debug("[%ld: Master] UNEXPECTED BEHAVIOUR while writing problem", (long)getpid());
            debug("[%ld: Master] Exiting the program", (long)getpid());
            exit(EXIT_FAILURE);
        }
        debug("[%ld:Master] Write data (fd = %d, nbytes = %ld)",
            (long)getpid(),master_write_fd,sizeof(struct problem));
        debug("[%ld: Master] Flush", (long)getpid());
        fflush(fd);
        debug("[%ld:Master] Workers alive: %d, stopped: %d, idle: %d"
            , (long)getpid(),workers_alive, workers_stopped,workers_idle);
        return;
    }else if ( worker_status == 4){
        //Worker is in running state :
        //possibilites
        //i) waiting to read a new problem
        //ii) send his result, both successs and failure
        //iii) if there are no more problems  and all workers are in idle state, simply return

       if (no_more_problems ==1  && total_workers ==workers_idle){
        return;
    }

    //Move running to stopped
    (worker_info+i)->status = 5;
    sf_change_state( (worker_info + i)->pid,4,5 );
    debug("[%ld:Master] Set state of worker %d (pid = %d): running->stopped",
        (long)getpid(),i,(worker_info+i)->pid);
    workers_stopped++;
    debug("[%ld:Master] Workers Stopped %d, current worker %d",(long)getpid(),workers_stopped,i);
    //Read Result
    int read_fd = (worker_info + i)-> master_read_fd;
    FILE* fd = fdopen(read_fd,"r");
    struct result* r = NULL;
    r =(struct result *) malloc(sizeof(struct result));
    debug("[%ld:Master] Read result (fd = %d)",(long)getpid(),read_fd);
    int count = fread(r, sizeof(char),sizeof(struct result), fd);
    if (count != sizeof(struct problem)){
        debug("[%ld: Master] UNEXPECTED BEHAVIOUR while reading header (read %d, expected %ld)",
           (long)getpid(),count, sizeof(struct result));
        debug("[%ld: Master] Exiting the program", (long)getpid());
        exit(EXIT_FAILURE);
    }
    debug("[%ld:Master] Read data (fd = %d, nbytes=%ld)",
        (long)getpid(),read_fd,sizeof(struct result));
    debug("[%ld:Master] Got result: size = %ld, failed = %d",
        (long)getpid(),r->size,r->failed);
    debug("[%ld:Master] Got result from worker %d, (pid = %d): size = %ld, failed = %d",
        (long)getpid(),i,(worker_info+i)->pid,r->size,r->failed);
    sf_recv_result((worker_info+i)->pid, r);
    //stopped to idle
    (worker_info+i)->status = 2;
    sf_change_state( (worker_info + i)->pid,5,2 );
    debug("[%ld:Master] Set state of worker %d (pid = %d): stopped->idle",
        (long)getpid(),i,(worker_info+i)->pid);
    workers_stopped--;
    workers_idle++;
    debug("[%ld:Master] Workers Stopped %d",(long)getpid(),workers_stopped);
    debug("[%ld:Master] Workers Idle %d",(long)getpid(),workers_idle);
    if (r->failed == 0){
        //Verify what u got as cuccess from the miner
        size_t result_size = r->size - sizeof(struct result);
        void* result_begin = (void *)(r)+sizeof(struct result);
        count = fread(result_begin, sizeof(char), result_size , fd);
        if (count != result_size) {
            debug("[%ld: Master] UNEXPECTED BEHAVIOUR while reading problem", (long)getpid());
        }
        struct problem* p =(worker_info+i)->problem_location;
        int result_status = -1;
        if((worker_info+i)-> problem_status != 1){

            result_status = post_result(r,p);
        }
        free(r);
        if (result_status == 0){
            //Mark problem as solved
            (worker_info+i)-> problem_status = 0;
            //search if there are other wrokers solving the same problem
            for(int j=0;j<total_workers;j++){
                if(i!=j && ((worker_info+j)->problem_id == (worker_info+i)->problem_id)){

                    (worker_info+j)-> problem_status = 1;
                    debug("[%ld:Master] Signaling worker %d (pid = %d) to abandon solved problem %p",
                        (long)getpid(),j,(worker_info+j)->pid,(worker_info+j)->problem_location);
                    sf_cancel((worker_info+j)->pid);
                    //send SIGHUP to cancdel that solution
                    debug("[%ld:Master] Sending SIGHUP to pid %d, it's current state is %d",
                        (long)getpid(),(worker_info+j)->pid, (worker_info+j)->status);
                    kill((worker_info+j)->pid,SIGHUP);
                }
            }
           // (worker_info+i)-> problem_location = NULL;
        }

    }else{
      //Failure case (or) when  master asked to cancel
      //pick up next problem or terminate
      free(r);
      (worker_info+i)-> problem_status = 0;
      //(worker_info+i)-> problem_location = NULL;
      debug("[%ld:Master] No more problems",(long)getpid());
      if (no_more_problems ==1 ){
        if(workers_idle == total_workers){
            for (int j=0; j<total_workers;j++){
                if((worker_info +j)->status == 2){
                    kill((worker_info+j)->pid,SIGTERM);
                    kill((worker_info+j)->pid,SIGCONT);
                    sf_change_state((worker_info+j)->pid,2,4);
                    (worker_info +j)->status  = 4;
                    debug("[%ld:Master] Set state of worker %d (pid = %d): idle->running",
                        (long)getpid(),i,(worker_info+i)->pid);
                }else{
                    debug("[%ld:Master] Worker %d is expected to be in idle(state =2 ) but its in state %d",
                        (long)getpid(),j,(worker_info+j)->status);
                }
            }
            return ;
        }
    }
    return;
}

// Now check for any other problems
struct problem* p= get_problem_variant(total_workers, i);
if(p== NULL){
    no_more_problems = 1;
    debug("[%ld:Master] Workers idle : %d",(long)getpid(), workers_idle);

    if (total_workers == workers_idle){
        for (int j=0; j<total_workers;j++){
            if((worker_info +j)->status == 2){
                kill((worker_info+j)->pid,SIGTERM);
                kill((worker_info+j)->pid,SIGCONT);
                sf_change_state((worker_info+j)->pid,2,4);
                (worker_info +j)->status  = 4;
                debug("[%ld:Master] Set state of worker %d (pid = %d): idle->running",
                    (long)getpid(),i,(worker_info+i)->pid);
            }else{
                debug("[%ld:Master] Worker %d is expected to be in idle(state =2 ) but its in state %d",
                    (long)getpid(),j,(worker_info+j)->status);
            }
        }
        debug("[%ld:Master] All live workers are idle -- terminating",(long)getpid());
        return;
    }
}else{
    //Ready to take next problem
    //Send SIGCONT to this worker he will pick up new problem
    (worker_info+i)->problem_location = p ;
    (worker_info+i)->problem_id = p->id;
    (worker_info+i)->problem_status = 0;
    debug("[%ld:Master]Worker %d Problem %p",(long)getpid(),i, (worker_info+i)->problem_location);
    kill((worker_info+i)->pid,SIGCONT);
    //idle to continue
    (worker_info+i)->status = 3;
    sf_change_state( (worker_info + i)->pid,2,3 );
    workers_idle--;
    debug("[%ld:Master] Set state of worker %d (pid = %d): idle->continued",
        (long)getpid(),i,(worker_info+i)->pid);
    debug("[%ld:Master] Assigning problem %p worker %d (pid = %d)",
        (long)getpid(),p,i,(worker_info + i)->pid);
}
}
return;
}



int handler(){
    // save old errno
    int olderrno = errno;
    int worker_status;
    sigset_t current_mask, prev_mask;
    if(sigchild_flag == 1){
        sigfillset(&current_mask);
        // Mask all signals
        sigprocmask(SIG_BLOCK, &current_mask, &prev_mask);
        pid_t worker_pid;
        while((worker_pid = waitpid(-1,&worker_status, WNOHANG | WUNTRACED | WCONTINUED)) > 0){

            int i =get_index_from_pid(worker_pid);
            if( i == -1){
                debug("[%ld: Master] Exiting the program", (long)getpid());
                exit(EXIT_FAILURE);
            }

            //Normal termination
            if(WIFEXITED(worker_status)){
                sf_change_state((worker_info+i)->pid,(worker_info+i)->status,6);
                debug("[%ld:Master] Set state of worker %d (pid = %d): running->exited",
                    (long)getpid(),i,(worker_info+i)->pid);
                workers_alive --;
                debug("[%ld:Master] Workers alive %d", (long)getpid(),workers_alive);
                if (worker_status!=0){
                    atleast_one_worker_had_issues =1;
                }
            }

            //Child Sends SIGSTOP
            if(WIFSTOPPED(worker_status)){
                debug("[%ld:Master]:Worker (pid = %d, state = %d) has stopped",
                    (long)getpid(),(worker_info +i)->pid,(worker_info +i)->status);
                perform_action_for_sig_child(i);
            }

            //Child Sends SIGCONT
            if(WIFCONTINUED(worker_status)){
                perform_action_for_sig_child(i);
            }

            //For Other Signals
            if(WIFSIGNALED(worker_status)){
                //Ignore SIGPIPE

               if((worker_info+i)->status != 2){
                workers_idle++;
               }
               workers_alive--;
               (worker_info+i)->status = 7;
               (worker_info+i)->problem_location = NULL;
               debug("[%ld:Master] Workers alive %d", (long)getpid(),workers_alive);
               debug("[%ld:Master]:Worker (pid = %d, state = %d) has terminated with signal (signal = %d)",
                (long)getpid(),(worker_info +i)->pid,(worker_info +i)->status,WTERMSIG(worker_status));
               sf_change_state((worker_info+i)->pid,(worker_info+i)->status,7);
               debug("[%ld:Master] Set state of worker %d (pid = %d): changed to aborted",
                (long)getpid(),i,(worker_info+i)->pid);
               atleast_one_worker_had_issues =1;

           }
       }
   }
 //Reset the flag
   sigchild_flag = 0;
   sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    //restore errorno
   errno = olderrno;
   return 0;
}

void sigchild_handler(int sig){
    sigchild_flag = 1 ;
    debug("[%ld:Master] Received signal %d", (long)getpid(),sig);
    return;
}

void sigpipe_handler(int sig){
    debug("[%ld:Master] Ignorong SIGPIPE handler, Received signal %d", (long)getpid(),sig);
    return;
}


int master(int workers) {
    // Master Initialization
    signal(SIGCHLD, sigchild_handler);
    signal(SIGPIPE, sigpipe_handler);

    //set signal mask for sigsuspend
    sigset_t mask;
    //Allow all signals
    sigemptyset(&mask);

    sf_start();
    worker_info = (struct worker_info*)malloc(workers * sizeof(struct worker_info));
    total_workers = workers;

    for(int i=0;i<workers;i++){

        debug("[%ld:Master] Starting Worker %d", (long)getpid(),i);

        (worker_info+i)->status = -1;
        (worker_info+i)->pid = -1;
        (worker_info+i)-> master_read_fd = -1;
        (worker_info+i)-> master_write_fd = -1;
        (worker_info+i)-> problem_id = -1;
        (worker_info+i)-> problem_status = -1;
        (worker_info+i)->problem_location = NULL;
        //Master to Worker
        //Close Worker Write
        //Close Master Read
        int fd1[2];
        pipe(fd1);
        (worker_info+i)-> master_write_fd = fd1[WRITE_END];

        //Worker to Master
        //Close Worker Read
        //Close Master Write
        int fd2[2];
        pipe(fd2);
        (worker_info+i)-> master_read_fd = fd2[READ_END];

        pid_t pid = fork() ;
        if(pid<0){
            debug("[%ld:Master] Error from fork, exiting the program", (long)getpid());
            exit(EXIT_FAILURE);
        }
        if(pid!= 0){

            //Master to Worker
            //Close Master Read
            close(fd1[READ_END]);

            //Worker to Master
            //Close Master Write
            close(fd2[WRITE_END]);

            (worker_info+i)->status = WORKER_STARTED;
            (worker_info+i)->pid = pid;
            sf_change_state(pid,0,1 );
            debug("[%ld:Master] Set state of worker %d (pid = %d): init->started",
                (long)getpid(),i,(worker_info+i)->pid);
            workers_alive++;
            debug("[%ld:Master] Workers alive %d", (long)getpid(),workers_alive);
            debug("[%ld:Master] Started worker 0 (pid = %d, in = %d, out = %d)",
                (long)getpid(),(worker_info+i)->pid, fd1[READ_END],fd2[WRITE_END]);


        }else{
            //Worker

            //Master to Worker
            //Close Worker Write
            close(fd1[WRITE_END]);
            if(dup2(fd1[READ_END], STDIN_FILENO)<0){
                debug("[%ld:Master] Error from dup2, exiting the program", (long)getpid());
                exit(EXIT_FAILURE);
            }

            //Worker to Master
            //Close Worker Read
            close(fd2[READ_END]);
            if(dup2(fd2[WRITE_END], STDOUT_FILENO)<0){
                debug("[%ld:Master] Error from dup2,exiting the program", (long)getpid());
                exit(EXIT_FAILURE);
            }
            execl("bin/polya_worker","bin/polya_worker",NULL);
        }
    }

    debug("[%ld:Master] Workers alive: %d, stopped: %d, idle: %d"
        , (long)getpid(),workers_alive, workers_stopped,workers_idle);

    while(1){
        if (workers_alive == 0){
            free(worker_info);
            sf_end();
            if (atleast_one_worker_had_issues == 0){
                debug("[%ld:Master] All problems are solved and no workers failed",(long)getpid());
                exit(EXIT_SUCCESS);

            }else{
                if(no_more_problems == 1){
                    debug("[%ld:Master] Atlreast one Worker did not terminate successfully",(long)getpid());
                }else{
                    debug("[%ld:Master] Not all the problems were solved",(long)getpid());
                }

                exit(EXIT_FAILURE);
            }
        }

        sigsuspend(&mask);
        if(sigchild_flag == 1){
            handler();
        }

        if(sigpipe_flag == 1){
            debug("[%ld:Master] SIGPIPE %d", (long)getpid(),sigchild_flag);
            sigpipe_flag = 0;
        }
    }

    return EXIT_SUCCESS;
}
