#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include "utils/_aux.h"
#include "fs/config.h"
#include "utils/requests.h"
#include <signal.h>

char box_name[MAX_BOX_NAME];
char sub_pipe_name[MAX_CLIENT_PIPE_NAME];
int message_count = 0;
int sub_pipe = -2;

static void sig_handler(int sig) {

  if (sig == SIGINT) {
    signal(SIGINT, SIG_DFL);
    if(close(sub_pipe)==-1) { //closing session
        fprintf(stderr,"Failed to close pipe(%s): %s\n", sub_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);  
    }
    printf("\n%d\n", message_count);
    raise(SIGINT);
    return; 
  }

}

void wait_for_messages() { // wait for publisher messages
    sub_pipe = open(sub_pipe_name, O_RDONLY);
        if(sub_pipe == -1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", sub_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);        
    }
    char buffer[MAX_MESSAGE_SIZE+5] = "";
    bool first = true;
    size_t size_of_buffer = 0; //so we can keep the size of the buffer after reading it before we split it
    while(true){
        
        ssize_t ret = read(sub_pipe, buffer, MAX_MESSAGE_SIZE + 4);
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", sub_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        size_of_buffer = strlen(buffer);
        if(first){
            // need to count how many messages where in the buffer at the first time
            for(int i = 0; i < size_of_buffer; i++){
                if(buffer[i]=='\n')message_count++;
            }
            first = false;
            // since the last one will be increased when printing, need to decrease 1 the counter
            message_count--;
        }
        //read something
        char *code_received=strtok(buffer, "|");
        if(atoi(code_received)==RECEIVED_MSG){
            char *message=strtok(NULL, "\0");
            while(message!=NULL){ // read all input messages until the end
                message_count++;
                fprintf(stdout, "%s", message);
                message=strtok(NULL, "\0");
            }
        }
        memset(buffer, '\0', size_of_buffer);
    }
}

int main(int argc, char **argv) {

    if( argc > 4 ) {
        fprintf(stderr,"Too many arguments supplied.\n");
        exit(EXIT_FAILURE);
    }
    else if(argc < 4 ) {
        fprintf(stderr,"Four argument expected.\n");
        exit(EXIT_FAILURE);
    }
    else if( argc == 4 ) {
        char register_pipe[MAX_CLIENT_PIPE_NAME];
        strcpy(register_pipe, argv[1]);
        strcpy(sub_pipe_name, argv[2]);
        strcpy(box_name, argv[3]);
        send_request(SUBSCRIBER, register_pipe, sub_pipe_name, box_name);
        if(check_connected(sub_pipe_name)==-1){ //check if sub connected to box specified in input
            printf("Couldn't connect to: %s\n", box_name);
            exit(EXIT_FAILURE);
        }
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        wait_for_messages();
    }        
        
    return -1;
}
