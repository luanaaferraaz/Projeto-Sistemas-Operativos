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

#define BUFFER_SIZE (MAX_ERROR_MESSAGE+5)

char pub_pipe_name[MAX_CLIENT_PIPE_NAME];
char box_name[MAX_BOX_NAME];
char *msg_from_pub = "9"; 

// check if user entered EOF in stdin
// gets the first character from the input and checks if it is EOF,
// if it isn't EOF puts the charater back because it may be part of the
// message the pub wrote.
int check_for_EOF() {
    if (feof(stdin)) return 1;
    int c = getc(stdin);
    if (c == EOF) return 1;
    ungetc(c, stdin);
    return 0;
}

void send_message_to_mb(int pub_pipe, char *message){

  size_t len = strlen(message);
  size_t written = 0;
  while (written < len) {

      ssize_t ret = write(pub_pipe, message + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }

}

void wait_for_messages(){ // wait for input messages
    char *reading = NULL;
    int pub_pipe = 0;
    bool wrote_message = false;
    //while it doesn´t read EOF from the input, runs
    while(!check_for_EOF()){
        size_t len = 0;
        ssize_t lineSize = 0;
        //gets a line
        lineSize = getline(&reading, &len, stdin);
        if(lineSize > 0) {
            char message[1024]="";
           
           //concatenates the message in a buffer with sending message code - 9
            strcat(message, msg_from_pub);
            strcat(message, "|");
            strcat(message, reading);
            
            //if it´s the first time writing to the pipe, needs to open it
            if(!wrote_message){
                pub_pipe = open(pub_pipe_name, O_WRONLY); //TO DO é preciso alguem à espera de read desta pipe, a thread
                if(pub_pipe==-1){
                    fprintf(stderr, "Failed to open: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            wrote_message = true;
            send_message_to_mb(pub_pipe, message);
            free(reading);
        }
    }
    // EOF closing session, need to close the pipe if it was opened
    if(wrote_message  && close(pub_pipe)==-1) { 
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
        wait_for_messages();
    }

    return 0;

}
