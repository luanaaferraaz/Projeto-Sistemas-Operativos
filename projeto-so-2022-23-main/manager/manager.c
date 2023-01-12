#include "logging.h"
#include "string.h"
#include "fs/config.h"
#include "utils/requests.h"
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include<unistd.h>

#define BUFFER_SIZE (MAX_ERROR_MESSAGE+10)

char manager_pipe_name[MAX_CLIENT_PIPE_NAME];

void wait_for_response(int code){
    sleep(1);
    int res = open(manager_pipe_name, O_RDONLY);
    if(res==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", manager_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(true){
        char buffer[BUFFER_SIZE] = "";
        ssize_t ret = read(res, buffer, BUFFER_SIZE - 1);
        
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", manager_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        if(close(res)==-1){
            fprintf(stderr, "Failed to close(%s): %s\n", manager_pipe_name, strerror(errno));
        }
        char *code_received=strtok(buffer, "|");
        char *return_code=strtok(NULL, "|");
        //ensuring we got the answer for our request, ==0 to check if no error occured
        if(atoi(code_received)==code && atoi(return_code) == 0){
            fprintf(stdout, "OK\n");
        }else if(atoi(code_received)==code && atoi(return_code) == -1){
            char *error_message = strtok(NULL, "|");
            fprintf(stdout, "ERROR %s\n", error_message);
        }
        //se for p/ continuar no while:
        /*if((res=open(manager_pipe_name, O_RDONLY))==-1){
            fprintf(stderr,"Failed to open pipe(%s): %s\n", manager_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE);
        }
        tirar o break*/
        break;
    }
}

void wait_for_list(int code) {

    sleep(1);
    int res = open(manager_pipe_name, O_RDONLY);
    if(res==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", manager_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(true){
        char buffer[BUFFER_SIZE] = "";
        ssize_t ret = read(res, buffer, BUFFER_SIZE - 1);
        
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", manager_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        
        char *code_received=strtok(buffer, "|");
        char *str_last=strtok(NULL, "|");
        int last = atoi(str_last);
        char *box_name=strtok(NULL, "|");
        char empty[32];
        memset(empty, '\0', MAX_BOX_NAME);
        
        if(atoi(code_received)==code && last == 1 && strcmp(box_name, empty)==0){ //ultima caixa da listagem
            fprintf(stdout, "NO BOXES FOUND\n");
        } else if(atoi(code_received)==code){
            char *box_size=strtok(NULL, "|");
            char *n_publishers=strtok(NULL, "|");
            char *n_subscribers=strtok(NULL, "|");
            fprintf(stdout, "%s %s %s %s\n", box_name, box_size, 
            n_publishers, n_subscribers);
        }
        if(last == 1) { break; }
    }
    if(close(res)==-1){
        fprintf(stderr, "Failed to close(%s): %s\n", manager_pipe_name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
}

int main(int argc, char **argv) {
    if( argc == 5 ) {
        if(strcmp(argv[3], "create")==0){
            char box_name[MAX_BOX_NAME];
            //int mode = CREATE_MANAGER;
            char register_pipe[MAX_CLIENT_PIPE_NAME];
            strcpy(register_pipe, argv[1]);
            strcpy(manager_pipe_name, argv[2]);
            strcpy(box_name, argv[4]);
            send_request(CREATE_MANAGER, register_pipe, manager_pipe_name, box_name);
            wait_for_response(4);

        } else if(strcmp(argv[3], "remove")==0){
            char box_name[MAX_BOX_NAME];
            //int mode = REMOVE_MANAGER;
            char register_pipe[MAX_CLIENT_PIPE_NAME];
            strcpy(register_pipe, argv[1]);
            strcpy(manager_pipe_name, argv[2]);
            strcpy(box_name, argv[4]);
            send_request(REMOVE_MANAGER, register_pipe, manager_pipe_name, box_name);
            wait_for_response(6);            

        } else {
            fprintf(stderr, "Incorrect argument\n");
            exit(EXIT_FAILURE);
        }
    }
    else if( argc == 4 ) {
        if(strcmp(argv[3], "list")==0){
            //int mode = LIST_MANAGER;
            char register_pipe[MAX_CLIENT_PIPE_NAME];
            strcpy(register_pipe, argv[1]);
            strcpy(manager_pipe_name, argv[2]);
            send_request_list(register_pipe, manager_pipe_name);
            wait_for_list(8);
        } else {
            fprintf(stderr, "Incorrect argument\n");
            exit(EXIT_FAILURE);
        }
    }
    else {
        printf("Correct argument expected.\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}
