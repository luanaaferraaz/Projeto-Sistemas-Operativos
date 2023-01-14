#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include "fs/config.h"
#include "requests.h"

char server_pipe_name[MAX_CLIENT_PIPE_NAME];
int write_message(int pipe, char *buffer){
    size_t len = strlen(buffer);
    size_t written = 0;
    while (written < len) {

        ssize_t ret = write(pipe, buffer + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "Failed to write: %s\n", strerror(errno));
            return -1;
        }
        written += (size_t)ret;
    }
    return 0;
}


void send_error(int pipe, char *code, char *return_code, char *error_message) {
  char buffer[MAX_ERROR_MESSAGE+20] = "";
  strcpy(buffer, code);
  strcat(buffer, "|");
  strcat(buffer, return_code);
  strcat(buffer, "|");
  strcat(buffer, error_message);

  if(write_message(pipe, buffer)==-1) exit(EXIT_FAILURE);
  /*size_t len = strlen(buffer);
  size_t written = 0;
  while (written < len) {

      ssize_t ret = write(pipe, buffer + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }*/
  
}

void send_msg_request_list(int pipe, char *code, char *client_pipe_name){
  char buffer[MAX_CLIENT_PIPE_NAME+20] = "";
  strcat(buffer, code);
  strcat(buffer, "|");
  strcat(buffer, client_pipe_name);
  size_t len = strlen(buffer);
  size_t written = 0;

  while (written < len) {
      ssize_t ret = write(pipe, buffer + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }
}

// wire protocol - write on pipe and made the interaction between server and clients with pipe messages
void send_msg_request(int pipe, char *code, char *client_pipe_name, char* box_name) {

  char buffer[MAX_CLIENT_PIPE_NAME+MAX_BOX_NAME+20] = "";
  strcat(buffer, code);
  strcat(buffer, "|");
  strcat(buffer, client_pipe_name);
  strcat(buffer, "|");
  strcat(buffer, box_name);
  size_t len = strlen(buffer);
  size_t written = 0;

  while (written < len) {
      ssize_t ret = write(pipe, buffer + written, len - written);
      if (ret < 0) {
          fprintf(stderr, "Failed to write: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
      }
      written += (size_t)ret;
  }
}


// receive the code of the action and call send_msg_request with opened pipe and the respective code
void send_request(int code, char* register_pipe_name, char *client_pipe_name, char *box_name) {

    memset(register_pipe_name + strlen(register_pipe_name), '\0', 
    sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(register_pipe_name)) -1);
    memset(client_pipe_name + strlen(client_pipe_name), '\0', 
    sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(client_pipe_name)) -1);
    memset(box_name + strlen(box_name), '\0', 
    sizeof(char)*(MAX_BOX_NAME - strlen(box_name)) -1);

    int pub_pipe = open(register_pipe_name, O_WRONLY);
    if(pub_pipe==-1){
      fprintf(stderr, "Failed to open: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    if(code == PUBLISHER) {
      send_msg_request(pub_pipe, "1", client_pipe_name, box_name);
    }
    else if(code == SUBSCRIBER) {
      send_msg_request(pub_pipe, "2", client_pipe_name, box_name);
    }
    else if(code == CREATE_MANAGER) {
      send_msg_request(pub_pipe, "3", client_pipe_name, box_name); 
    }
    else if(code == REMOVE_MANAGER) {
      send_msg_request(pub_pipe, "5", client_pipe_name, box_name); 
    }
    if(close(pub_pipe)==-1){
      fprintf(stderr, "Failed to close: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
}

void send_request_list(char* register_pipe_name, char *client_pipe_name){
    memset(register_pipe_name + strlen(register_pipe_name), '\0', sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(register_pipe_name)) -1);
    //verificar se o client pipe name ja existe
    memset(client_pipe_name + strlen(client_pipe_name), '\0', sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(client_pipe_name)) -1);

    int pub_pipe = open(register_pipe_name, O_WRONLY);
    if(pub_pipe==-1){
      fprintf(stderr, "Failed to open: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    send_msg_request_list(pub_pipe, "7", client_pipe_name); //list e falta a respostaaaaaaaaaaaaaaaaaa---------------
}

void send_response(char* code, char* client_name, int return_code, char* error_message) {
  memset(client_name + strlen(client_name), '\0', sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(client_name)) -1);
  puts("will open pipe in send_response");
  int message_pipe = open(client_name, O_WRONLY);
  if(message_pipe==-1){
    fprintf(stderr, "Failed to open: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  puts("opened pipe in send _response");
  memset(error_message + strlen(error_message), '\0', sizeof(char)*(MAX_ERROR_MESSAGE - strlen(error_message) -1));
  if(return_code == -1) {
    send_error(message_pipe, code, "-1", error_message);
  }
  else if(return_code == 0) {
    send_error(message_pipe, code, "0", error_message);
  }
  
  
}