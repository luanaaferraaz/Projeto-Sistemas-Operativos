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

#define BUFFER_SIZE (MAX_ERROR_MESSAGE+5)

char box_name[MAX_BOX_NAME];
char sub_pipe_name[MAX_CLIENT_PIPE_NAME];

void wait_for_messages(){
    //talvez esperar por um signal
    int pipe_on = open(sub_pipe_name, O_RDONLY);
    if(pipe_on == -1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", sub_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);        
    }
    puts("waiting for messages");
    while(true){
        char buffer[BUFFER_SIZE] = "";
        ssize_t ret = read(pipe_on, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", sub_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        //read something
        char *code_received=strtok(buffer, "|");
        if(atoi(code_received)==RECEIVED_MSG){
            char *message=strtok(NULL, "\0");
            while(message!=NULL){
                fprintf(stdout, "%s\n", message);
                message=strtok(NULL, "\0");
            }
        }
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
        if(check_connected(sub_pipe_name)==-1){
            printf("Couldn't connect to: %s\n", box_name);
            exit(EXIT_FAILURE);
        }
        wait_for_messages();
    }        
        

    return -1;
}
