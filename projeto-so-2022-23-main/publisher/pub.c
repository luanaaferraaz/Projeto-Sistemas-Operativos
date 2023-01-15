#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "fs/config.h"
#include "utils/requests.h"
#include "utils/_aux.h"
#include <signal.h>

char pub_pipe_name[MAX_CLIENT_PIPE_NAME];
char box_name[MAX_BOX_NAME];
char *msg_from_pub = "9"; 
int pub_pipe = 0;

static void sig_handler(int sig) {

  if (sig == SIGPIPE) {
    signal(SIGPIPE, SIG_DFL);
    if(close(pub_pipe)==-1) { //closing session
        fprintf(stderr,"Failed to close pipe(%s): %s\n", pub_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);  
    }
    raise(SIGINT);
    return; 
  }

}

int check_for_EOF() {
    if (feof(stdin)) return 1;
    int c = getc(stdin); // get first charater
    if (c == EOF) return 1;
    ungetc(c, stdin); //if it is not EOF puts the charater back because it may be part of the message the pub wrote
    return 0;
}

void wait_for_messages(){ // wait for input messages
    char *reading = NULL;
    pub_pipe = open(pub_pipe_name, O_WRONLY); 
    if(pub_pipe==-1){
        fprintf(stderr, "Failed to open: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(!check_for_EOF()){
        size_t len = 0;
        ssize_t lineSize = 0;
        //gets a line
        lineSize = getline(&reading, &len, stdin);
        if(lineSize > 0) {
            char message[MAX_MESSAGE_SIZE]="";
           //concatenates the message in a buffer with sending message code - 9
            strcat(message, msg_from_pub);
            strcat(message, "|");
            strcat(message, reading);
            
            if(write_message(pub_pipe, message)==-1){
                break;
                free(reading);
            }
        }
    }
    //needs to send a message to the pipe informing it is closing
    char message[3] = "13";
    
    write_message(pub_pipe, message);
    // EOF closing session, need to close the pipe 
    if(close(pub_pipe)==-1) { 
        fprintf(stderr,"Failed to close pipe(%s): %s\n", pub_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);  
    }
}

int main(int argc, char **argv) {

    if( argc > 4 ) {
        fprintf(stderr, "Too many arguments supplied.\n");
        exit(EXIT_FAILURE);
    }
    else if(argc < 4 ) {
        fprintf(stderr, "Four argument expected.\n");
        exit(EXIT_FAILURE);
    }
    else if( argc == 4 ) {
        char register_pipe[MAX_CLIENT_PIPE_NAME];
        strcpy(register_pipe, argv[1]);
        strcpy(pub_pipe_name, argv[2]); //client_pipe_name
        strcpy(box_name, argv[3]);

        //writes the request (for the pub to connect to the box) in server_pipe
        send_request(PUBLISHER, register_pipe, pub_pipe_name, box_name);
        
        //check if sub connected to box specified in input
        if(check_connected(pub_pipe_name)==-1){ 
            exit(EXIT_FAILURE);
        }
        if (signal(SIGPIPE, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        wait_for_messages();
    }

    return 0;

}
