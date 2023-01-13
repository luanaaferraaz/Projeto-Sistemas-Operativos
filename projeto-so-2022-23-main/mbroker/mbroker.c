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
#include "producer-consumer.h"

#define BUFFER_SIZE (MAX_CLIENT_PIPE_NAME+MAX_BOX_NAME+4)
#define MAX_BOXES (1024)
char *creating_manager="3", *removing_manager="5", *listing_manager="7", 
*pub = "1", *sub = "2", *create_box_err="4", *remove_box_err="6"; 

char server_pipe_name[MAX_CLIENT_PIPE_NAME];

pthread_cond_t box_signals[MAX_BOXES];
pthread_mutex_t box_locks[MAX_BOXES];

struct box_info{ 
    char box_name[MAX_BOX_NAME];
    int pub_counter;
    int sub_counter;
    char manager_name[MAX_CLIENT_PIPE_NAME];
};

struct box_info boxes[MAX_BOXES];

int write_message(char *message, char *box_name){ // write message in box (write on a tfs file)
    int handler = 0;
    if((handler = tfs_open(box_name, TFS_O_APPEND))==-1){
        fprintf(stderr, "[ERR]: Failed to open box (%s): %s\n", box_name,
                            strerror(errno));
        return -1;
    }
    strcat(message, "\0");

    ssize_t written = 0;
    if((written = tfs_write(handler, message, strlen(message)))==-1){
        fprintf(stderr, "[ERR]: Failed to write (%s) in the box (%s): %s\n", message, box_name,
                            strerror(errno));
        return -1;
    } 
    if(tfs_close(handler)==-1){
        fprintf(stderr, "[ERR]: closing (%s) failed: %s\n", box_name,
            strerror(errno));
        return -1;
    }
    return 0;
}

void work_with_sub(){
    char *client_name = strtok(NULL, "|");
    char *box_name = strtok(NULL, "|");
    bool connected = false;
    int box_index = -1;
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            boxes[i].sub_counter++;
            connected = true;
            box_index=i;
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
    //esperar por um sinal i guess
    int handler = 0;
    if((handler = tfs_open(box_name, 0))==-1){
        fprintf(stderr, "[ERR]: Failed to open box (%s): %s\n", box_name,
                            strerror(errno));
        exit(EXIT_FAILURE);
    }
    char buffer[MAX_ERROR_MESSAGE]="";
    ssize_t read = 0;
    while(true){
        if(pthread_mutex_lock(&box_locks[box_index])!=0){
            fprintf(stderr, "Failed to lock mutex: %s\n", strerror(errno));
        }
        printf("buffer: %s\n", buffer);
        while((read=tfs_read(handler, buffer, MAX_ERROR_MESSAGE-1)) <= 0){
            puts("waiting");
            printf("read --> %ld\n", read);
            pthread_cond_wait(&box_signals[box_index], &box_locks[box_index]);
            puts("got signal");
        }
        printf("read out of while: %lu\n", read);
        //read=tfs_read(handler, buffer, MAX_ERROR_MESSAGE-1);
        
        char message[MAX_ERROR_MESSAGE + 3];
        strcpy(message, "10");
        strcat(message, "|");
        strcat(message, buffer);
        int pipe_on = open(client_name, O_WRONLY);
        if(pipe_on == -1){
            fprintf(stderr,"Failed to open pipe(%s): %s\n", client_name,
                    strerror(errno));
            exit(EXIT_FAILURE);        
        }
        size_t len = strlen(message);
        size_t written = 0;
        while (written < len) {
            ssize_t ret = write(pipe_on, message + written, len - written);
            if (ret < 0) {
                fprintf(stderr, "Failed to write: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            written += (size_t)ret;
        }
        if(close(pipe_on)==-1){
            fprintf(stderr,"Failed to close pipe(%s): %s\n", client_name,
                    strerror(errno));
            exit(EXIT_FAILURE);  
        }
        if(pthread_mutex_unlock(&box_locks[box_index])!=0){
            fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(errno));
        }
        puts(buffer);
        memset(buffer, '\0', strlen(buffer));

    }
}

void work_with_pub(){
    char *client_name = strtok(NULL, "|");
    char *box_name = strtok(NULL, "|");
    bool connected = false;
    int box_index = -1;
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0 && boxes[i].pub_counter == 0) {
            boxes[i].pub_counter++;
            connected = true;
            box_index = i;
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
    
    int rx = open(client_name, O_RDONLY);
    if(rx==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", client_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(true){
        char buffer[BUFFER_SIZE] = "";
        ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            continue;
        }else if(ret==-1){
            fprintf(stderr,"Failed to read from pipe(%s): %s\n", client_name,
                strerror(errno));
            exit(EXIT_FAILURE); 
        }
        //read something
        char *code_received=strtok(buffer, "|");
        if(atoi(code_received)==9){
            char *message=strtok(NULL, "\0");
            if(pthread_mutex_lock(&box_locks[box_index])!=0){
                fprintf(stderr, "Failed to lock mutex: %s\n", strerror(errno));
                //largar thread
            }
            if(write_message(message, box_name)==-1){
                if(pthread_mutex_unlock(&box_locks[box_index])!=0){
                    fprintf(stderr, "Failed to lock mutex: %s\n", strerror(errno));
                }
                //largar a thread
            }else{
                if(pthread_mutex_unlock(&box_locks[box_index])!=0){
                    fprintf(stderr, "Failed to lock mutex: %s\n", strerror(errno));
                }
                //largar a thread
                pthread_cond_broadcast(&box_signals[box_index]);
            }

        }
    }
}

void work_with_manager_listing(){
    char *client_name = strtok(NULL, "|");
    if (unlink(client_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
        strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((mkfifo(client_name, 0640))!=0){
        fprintf(stderr,"Failed to create fifo here.\n");
        exit(EXIT_FAILURE);
    }
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
        if(strcmp(boxes[i].box_name, "") != 0 && strcmp(boxes[i].manager_name, client_name)==0) {
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

// check if create the box give an error and send error message(that is "" if noting wrong happened) to send_response
void work_with_manager_creating(char *client_name, char *box_name){
    int return_code = 0; // return code indicates if the box is created with success
    char error_message[MAX_ERROR_MESSAGE] = "\0";
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            if(strcmp(boxes[i].manager_name, client_name)!=0){ //  box_name is equal to other box so we check if it is in a different manager
                if (unlink(client_name) != 0 && errno != ENOENT) {
                    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_name,
                    strerror(errno));
                    exit(EXIT_FAILURE);
                }
                if((mkfifo(client_name, 0640))!=0){
                    fprintf(stderr,"Failed to create fifo here.\n");
                    exit(EXIT_FAILURE);
                }
                strcpy(error_message, "Box already exists associated with another manager: ");
                strcat(error_message, box_name);
            }else{ // if is equal we try to create a box at the same manager, wich is impossible
                strcpy(error_message, "Duplicated box: ");
                strcat(error_message, box_name);
            }
            return_code = -1;
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
            if(strcmp(boxes[i].box_name, "") == 0) { // put the new box in the first element of array that is available
                strcpy(boxes[i].box_name,box_name);
                boxes[i].pub_counter = 0;
                boxes[i].sub_counter = 0;
                strcpy(boxes[i].manager_name, client_name);
                pthread_mutex_init(&box_locks[i], NULL);
                pthread_cond_init(&box_signals[i], NULL);
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

// just check if remove the box give an error and send error message (that is "" if noting wrong happened) to send_response
void work_with_manager_removing(char *client_name, char *box_name) { 

    int return_code = -1;
    char error_message[MAX_ERROR_MESSAGE] = "\0";
    int removed = -1; // initialize removed as if the box do not exist
    for(int i = 0; i < MAX_BOXES; i++) {
        if(strcmp(boxes[i].box_name, box_name) == 0) {
            removed = -2; // if the box exist change removed value
            if(strcmp(boxes[i].manager_name, client_name)==0){
                int file_box = tfs_unlink(box_name);
                if(file_box == -1) {
                    return_code = -1;
                    strcpy(error_message, "Failed to remove box: ");
                    strcat(error_message, box_name);
                }
                removed = 0; // if client_name is the same the box are not associated with another manager, so creating the box is ok
                strcpy(boxes[i].box_name, "");
                boxes[i].pub_counter = 0;
                boxes[i].sub_counter = 0;
                strcpy(boxes[i].manager_name, "");
                pthread_mutex_destroy(&box_locks[i]);
                pthread_cond_destroy(&box_signals[i]);
                return_code = 0;
            }
            break;
        }
    }
    //found the box, however cannot remove it, it is associated with another manager
    if(removed == -2){
        strcpy(error_message, "Box (");
        strcat(error_message, box_name);
        strcat(error_message, ") associated with another manager.");
    } else if(removed == -1) {
        strcpy(error_message, "Box (");
        strcat(error_message, box_name);
        strcat(error_message, ") does not exist.");
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

void *work() { // read buffer and with is code choose wich action to do
    int rx = open(server_pipe_name, O_RDONLY);
    if(rx==-1){
        fprintf(stderr,"Failed to open pipe(%s): %s\n", server_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(true){
        
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

        else if(strcmp(code, creating_manager) == 0 || 
            strcmp(code, removing_manager) == 0) { //create or remove box   
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
        
        buffer[ret] = 0;
    }
}

void createThreads(int max_sessions) { // create all (max_sessions) threads at the beginning of the program
    pthread_t tid[max_sessions];

    for(int i=0; i<max_sessions; i++){
        assert(pthread_create(&tid[i], NULL, &work, &i)==0);
    }
    for(int i=0; i<max_sessions; i++){
        assert(pthread_join(tid[i], NULL)==0);
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
          fprintf(stderr,"Failed to create fifo.\n");
          exit(EXIT_FAILURE);
        }

        char max_sessions_str[50]; 
        strcpy(max_sessions_str, argv[2]);
        int max_sessions = atoi(max_sessions_str);
        tfs_init(NULL);

        pc_queue_t *queue = malloc(sizeof(pc_queue_t) * (size_t)(max_sessions*2));
        if(pcq_create(queue, (size_t)(max_sessions*2)) == -1) {
            fprintf(stderr,"Failed to create queue.\n");
            exit(EXIT_FAILURE);
        }
        work(queue);
        createThreads(max_sessions);
      
    }

      return 0;

}