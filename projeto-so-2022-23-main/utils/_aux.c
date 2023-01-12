#include "_aux.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int write_connected_message(int pipe, char *message){
  size_t len = strlen(message);
  size_t written = 0;

  while (written < len) {

      ssize_t ret = write(pipe, message + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }
  if(close(pipe)==-1){
    return -1;
  }
  return 0;
}

void send_connected_msg(char *client_name, bool connected){
    int rd = open(client_name, O_WRONLY);
    if(rd==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", client_name,
        strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (connected){
        char message[2]= "0";
        if(write_connected_message(rd, message)==-1){
            fprintf(stderr, "Failed to close(%s): %s\n", client_name, strerror(errno));
        }            
    } else {
        char message[3]= "-1";
        if(write_connected_message(rd, message)==-1){
            fprintf(stderr, "Failed to close(%s): %s\n", client_name, strerror(errno));
        }
    }
}

int check_connected(char *pipe_name){
    sleep(1);
    int pipe_on = open(pipe_name, O_RDONLY);
    if(pipe_on == -1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", pipe_name, strerror(errno));
        exit(EXIT_FAILURE);        
    }
    while(true){
        char buffer[3] = "";
        ssize_t ret = read(pipe_on, buffer, 3 - 1);
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        if(strcmp(buffer, "-1")==0){
            if(close(pipe_on)==-1){
                return -1;
            }
            return -1;
        }else if(strcmp(buffer, "0")==0){
            if(close(pipe_on)==-1){
                return -1;
            }
            return 0;            
        }
        break;
    }
    return -1;
}
