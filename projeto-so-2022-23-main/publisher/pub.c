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

void send_message_to_mb(char *message, char *pipe_name){
    //talvez dar um sinal para a thread no mbroker ler

  int pub_pipe = open(pipe_name, O_WRONLY); //TO DO é preciso alguem à espera de read desta pipe, a thread
  if(pub_pipe==-1){
    fprintf(stderr, "Failed to open--: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  size_t len = strlen(message);
  size_t written = 0;
  puts(box_name);
  while (written < len) {

      ssize_t ret = write(pub_pipe, message + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }
  //talvez um sinal para o mbroker a tratar disto escrever a 
  //mensagem ler da pipe e escrever no ficheiro no tfs
}

void wait_for_messages(){
    char *reading = NULL;
    while(true){
        puts("write a message:");
        size_t len = 0;
        ssize_t lineSize = 0;
        lineSize = getline(&reading, &len, stdin);
        if(lineSize > 0) {
            printf("got your message: %s\n", reading);
            send_message_to_mb(reading, pub_pipe_name);
            free(reading);
        }
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
        send_request(PUBLISHER, register_pipe, pub_pipe_name, box_name);
        if(check_connected(pub_pipe_name)==-1){
            exit(EXIT_FAILURE);
        }
        wait_for_messages();
    }

    return 0;

}
