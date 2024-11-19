#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

static void check_error(int ret, const char* error_call){
    if(ret != -1){
        return;
    }
    int err = errno;
    perror(error_call);
    exit(err);
}

//close all the file descriptors up to the current iteration 
static void close_fd(int process_pipes[][2], int num_of_pipes){

    for(int pipes = 0; pipes <= num_of_pipes; pipes++){
        check_error(close(process_pipes[pipes][1]), "close");
        check_error(close(process_pipes[pipes][0]), "close");
    }
}

static void child(int process_pipes[][2], int child_process_num, int total_child_process, const char* program){
    
    if(child_process_num == 0 && total_child_process == 1){//no pipes created here because no subsequent processes 
        if(execlp(program, program, NULL) == -1){
            //printf("Do i get here?\n");
            exit(errno);
        }
    }else if(child_process_num == 0 && total_child_process != 1){
        //keep stdin same as parent process
        check_error(dup2(process_pipes[0][1], STDOUT_FILENO), "dup2"); //replace write end of the first pipe with child process1's stdout
        //check_error(close(process_pipes[0][1]), "close");
        //check_error(close(process_pipes[0][0]), "close");
        close_fd(process_pipes, child_process_num);
    }else{

        check_error(dup2(process_pipes[child_process_num - 1][0], STDIN_FILENO), "dup2");
        //close_fd(process_pipes, child_process_num - 1);

        if(child_process_num + 1 != total_child_process){
            check_error(dup2(process_pipes[child_process_num][1], STDOUT_FILENO), "dup2");
            //close_fd(process_pipes, child_process_num - 1);
        }
        close_fd(process_pipes, child_process_num-1);
    }
    
    if(execlp(program, program, NULL) == -1)
        exit(errno);
}

int main(int argc, char *argv[]) {
    
    if(argc == 1){
        return EINVAL;
    }

    int process_pipes[argc-2][2]; //pipes between processes
    
    //printf("here\n");
    for(int i = 0; i < argc-2; i++){
        check_error(pipe(process_pipes[i]), "pipe");
    }

    //create new process
    //at time of fork we have 5 standard file descriptors 
    //only spawn new processes inside parent thread
    //create argc-1 number of new processes
    int process_spawned = argc - 1;
    pid_t process_library[process_spawned];
   
    //forking new child processes, store their ids in parent process_library
    for(int process_created = 0; process_created < process_spawned; process_created++){
        
        pid_t child_process =  fork();
        
        if((child_process < 0)){
            perror("fork");
            exit(errno);
        }
        else if(child_process == 0){
            
            //connect pipe only if between subsequent processes
            child(process_pipes, process_created, process_spawned, argv[process_created + 1]);
        }
        else{
            //keep track of child processes
            process_library[process_created] = child_process;
        }
    }
    
    //wait for child to exit 
    for(int num_child_process = 0; num_child_process < process_spawned; num_child_process++){
        
        if(process_spawned == 1){
            close(STDOUT_FILENO);
            close(STDIN_FILENO);
        }else{
            close(process_pipes[num_child_process][0]);
            close(process_pipes[num_child_process][1]);
        }
        
        int wait_status; //wait for child to finish 
        if(waitpid(process_library[num_child_process], &wait_status, 0) == -1)
            exit(errno);

        if(WIFEXITED(wait_status)){
            int exit_status =  WEXITSTATUS(wait_status);
            if(exit_status != 0)
                exit(exit_status);
        }else{
            return 0;
        }
    }
     
    return 0;
}
