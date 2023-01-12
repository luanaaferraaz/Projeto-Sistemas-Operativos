#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include "fs/config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils/requests.h"
#include "utils/_aux.h"
#include "fs/operations.h"

#define BUFFER_SIZE (MAX_CLIENT_PIPE_NAME+MAX_BOX_NAME+4)
#define MAX_BOXES (1024)
char *creating_manager="3", *removing_manager="5", *listing_manager="7", 
*pub = "1", *sub = "2", *create_box_err="4", *remove_box_err="6"; 

char server_pipe_name[MAX_CLIENT_PIPE_NAME];

void *non_active_wait(){return NULL;}

struct box_info{ 
    char box_name[MAX_BOX_NAME];
    int pub_counter;
    int sub_counter;
};

struct box_info boxes[MAX_BOXES];

void createThreads(int max_sessions){
    pthread_t tid[max_sessions];

    for(int i=0; i<max_sessions; i++){
        assert(pthread_create(&tid[i], NULL, non_active_wait, &i)==0);
    }
    for(int i=0; i<max_sessions; i++){
        assert(pthread_join(tid[i], NULL)==0);
    }
}

void work_with_sub(){
    char *client_name = strtok(NULL, "|");
    char *box_name = strtok(NULL, "|");
    bool connected = false;
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            boxes[i].sub_counter++;
            connected = true;
            break;
        }
    }
    if (unlink(client_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
        strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((mkfifo(client_name, 0640))!=0){
        fprintf(stderr,"Failed to create fifo here.\n");
        exit(EXIT_FAILURE);
    }
    
    send_connected_msg(client_name, connected);
}

void work_with_pub(){
    char *client_name = strtok(NULL, "|");
    char *box_name = strtok(NULL, "|");
    bool connected = false;
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0 && boxes[i].pub_counter == 0) {
            boxes[i].pub_counter++;
            connected = true;
            break;
        }
    }
    if (unlink(client_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
        strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((mkfifo(client_name, 0640))!=0){
        fprintf(stderr,"Failed to create fifo here.\n");
        exit(EXIT_FAILURE);
    }
    send_connected_msg(client_name, connected);
    //esperar por sinal p/ abrir a pipe e ler mensagens
}

void work_with_manager_listing(){
    char *client_name = strtok(NULL, "|");
    char aux_box_name[MAX_BOX_NAME];
    int aux_pub_counter;
    int aux_sub_counter;
    bool found = false;
    for(int i = 0; i < MAX_BOXES; i++){ //ordenar as boxes por ordem alfabética
        for(int j = i + 1; j < MAX_BOXES; j++){
            if(strcmp(boxes[i].box_name ,boxes[j].box_name)>0){
                strcpy(aux_box_name,boxes[i].box_name);
                aux_pub_counter = boxes[i].pub_counter;
                aux_sub_counter = boxes[i].sub_counter;

                strcpy(boxes[i].box_name,boxes[j].box_name);
                boxes[i].pub_counter = boxes[j].pub_counter;
                boxes[i].sub_counter = boxes[j].sub_counter;

                strcpy(boxes[j].box_name,aux_box_name);
                boxes[j].pub_counter = aux_pub_counter;
                boxes[j].sub_counter = aux_sub_counter;
            }
        }
    }
    
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, "") != 0) {
            found=true;
            long int n_publishers = 0, n_subscribers = 0;
            n_publishers = boxes[i].pub_counter;
            n_subscribers = boxes[i].sub_counter;
            char *box_name = boxes[i].box_name;
            int res = tfs_open(box_name, O_RDONLY);
            char reader[BUFFER_SIZE] = "";
            ssize_t box_size = tfs_read(res, reader, BUFFER_SIZE - 1);

            int man_pipe = open(client_name, O_WRONLY); 
            if(man_pipe==-1){
                fprintf(stderr, "Failed to open--: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            char message[2+2+MAX_BOX_NAME+20+2+2+1] = "";
            char last[2] = "0";
            if(i == MAX_BOXES -1 ) {strcpy(last,"1");}
            strcat(message, "8");
            strcat(message, "|");
            strcat(message, last);
            strcat(message, "|");
            strcat(message, box_name);
            strcat(message, "|");

            char str_box_size[20] = "";
            //printf("%zu\n", box_size);
            sprintf(str_box_size, "%zu", box_size);
            strcat(message, str_box_size);
            strcat(message, "|");

            char str_publishers[2] = "";
            sprintf(str_publishers, "%zu", n_publishers);
            strcat(message, str_publishers);
            strcat(message, "|");
            char str_subscribers[2] = "";
            sprintf(str_subscribers, "%zu", n_subscribers);
            strcat(message, str_subscribers);
            
            size_t len = strlen(message);
            size_t written = 0;
            while (written < len) {

                ssize_t ret = write(man_pipe, message + written, len - written);
                if (ret < 0) {
                    fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                written += (size_t)ret;
            }
            if(close(man_pipe)==-1){
                fprintf(stderr, "Failed to close(%d): %s\n", man_pipe, strerror(errno));
                exit(EXIT_FAILURE);
            }
            sleep(1);
            //talvez um sinal para o mbroker a tratar disto escrever a 
            //mensagem ler da pipe e escrever no ficheiro no tfs
            
        }
    }
    if(!found){
        int man_pipe = open(client_name, O_WRONLY); 
        if(man_pipe==-1){
            fprintf(stderr, "Failed to open--: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        char message[1024];
        strcpy(message, "8");
        strcat(message, "|");
        strcat(message, "1");
        strcat(message, "|");
        char box_name[MAX_BOX_NAME];
        memset(box_name, '\0', MAX_BOX_NAME);
        strcat(message, box_name);

        size_t len = strlen(message);
        size_t written = 0;
        while (written < len) {

            ssize_t ret = write(man_pipe, message + written, len - written);
            if (ret < 0) {
                fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            written += (size_t)ret;
        }
        if(close(man_pipe)==-1){
            fprintf(stderr, "Failed to close(%d): %s\n", man_pipe, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
}

void work_with_manager_creating(char *client_name, char *box_name){
    int return_code = 0;
    char error_message[MAX_ERROR_MESSAGE] = "\0";
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            return_code = -1;
            strcpy(error_message, "Duplicated box: ");
            strcat(error_message, box_name);
            break;
        }
    }
    if(return_code == 0) {
        int file_box = tfs_open(box_name,TFS_O_CREAT);
        if(file_box == -1) {
            return_code = -1;
            strcpy(error_message, "Failed to open box: ");
            strcat(error_message, box_name);
        }
        if(tfs_close(file_box)==-1){
            fprintf(stderr, "[ERR]: Failed to close box (%s): %s\n", client_name,
            strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        for(int i = 0; i < (MAX_BOX_NAME); i++) {
            if(strcmp(boxes[i].box_name, "") == 0) {
                strcpy(boxes[i].box_name,box_name);
                boxes[i].pub_counter = 0;
                boxes[i].sub_counter = 0;
                break;
            }
        }

        if (unlink(client_name) != 0 && errno != ENOENT) {
            fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
            strerror(errno));
            exit(EXIT_FAILURE);
        }
        if((mkfifo(client_name, 0640))!=0){
            fprintf(stderr,"Failed to create fifo here.\n");
            exit(EXIT_FAILURE);
        }
            
    }
    send_response(create_box_err, client_name, return_code, error_message);
    
}

void work_with_manager_removing(char *client_name, char *box_name){
    int return_code = 0;
    char error_message[MAX_ERROR_MESSAGE] = "\0";
    int removed = -1;
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            int file_box = tfs_unlink(box_name);
            if(file_box == -1) {
                return_code = -1;
                strcpy(error_message, "Failed to remove box: ");
                strcat(error_message, box_name);
            }
            removed = 0;
            strcpy(boxes[i].box_name, "");
            boxes[i].pub_counter = 0;
            boxes[i].sub_counter = 0;
            break;
        }
    }
    if(removed == -1) {
        strcpy(error_message, "Box (");
        strcat(error_message, box_name);
        strcat(error_message, ") does not exist.");
        return_code = -1;
    }
    if (unlink(client_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
        strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((mkfifo(client_name, 0640))!=0){
        fprintf(stderr,"Failed to create fifo here.\n");
        exit(EXIT_FAILURE);
    }
    send_response(remove_box_err, client_name, return_code, error_message);
                
}

void work(int max_sessions){
    int rx = open(server_pipe_name, O_RDONLY);
    if(rx==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", server_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    int count = 0;
    while(true){
        if(count>=max_sessions){
            //fila--------------------------------
        }
        char buffer[BUFFER_SIZE] = "";
        ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            continue;
        }else if(ret == -1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", server_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE);
        }
        if(close(rx)==-1){
            fprintf(stderr, "Failed to close(%s): %s\n", server_pipe_name, strerror(errno));
        }
        char *code=strtok(buffer, "|");
        
        if(strcmp(code, pub)==0){ work_with_pub();}

        
        if(strcmp(code, sub)==0){ work_with_sub();}

        else if(strcmp(code, creating_manager) == 0 || strcmp(code, removing_manager) == 0) { //create or remove box   
            char *client_name = strtok(NULL, "|");
            char *box_name = strtok(NULL, "|");
            
            if(strcmp(code, creating_manager) == 0) { work_with_manager_creating(client_name, box_name);}
            //if it is not creating, it is removing 
            else { work_with_manager_removing(client_name, box_name); }
        }
        else if(strcmp(code, listing_manager) == 0) { work_with_manager_listing(); }

        if((rx=open(server_pipe_name, O_RDONLY))==-1){
            fprintf(stderr,"Failed to open pipe(%s): %s\n", server_pipe_name,
            strerror(errno));
            exit(EXIT_FAILURE);
        }
        //-----------------falta respostassssss??????-------------
        
        buffer[ret] = 0;
    }
}

int main(int argc, char **argv) {
    if( argc > 3 ) {
        fprintf(stderr,"Too many arguments supplied.\n");
        exit(EXIT_FAILURE);
    }
    else if(argc < 3 ) {
        fprintf(stderr,"Tree argument expected.\n");
        exit(EXIT_FAILURE);
    }
    else if( argc == 3 ) {
        if(strlen(argv[1])>MAX_CLIENT_PIPE_NAME){
            fprintf(stderr,"Pipe name too long.\n");
            exit(EXIT_FAILURE);
        }
        strcpy(server_pipe_name, argv[1]);

        memset(server_pipe_name + strlen(server_pipe_name), '\0', sizeof(char)*(MAX_CLIENT_PIPE_NAME - strlen(server_pipe_name)) -1);

        if (unlink(server_pipe_name) != 0 && errno != ENOENT) {
            fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_name,
                strerror(errno));
            exit(EXIT_FAILURE);
        }

        if(mkfifo(server_pipe_name, 0640)!=0){
          fprintf(stderr,"Failed to create fifo here.\n");
          exit(EXIT_FAILURE);
        }

        char max_sessions_str[50]; 
        strcpy(max_sessions_str, argv[2]);
        int max_sessions = atoi(max_sessions_str);
        createThreads(max_sessions);
        tfs_init(NULL);
        work(max_sessions);
      
    }

      return 0;

}