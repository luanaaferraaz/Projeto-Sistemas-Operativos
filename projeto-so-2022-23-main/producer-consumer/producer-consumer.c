#include "producer-consumer.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REQUEST_SIZE (256+256+32+20) 
//2*256 for the pipes names, server pipe and session pipe
//32 for box name
//20 for |, code, an create/remove when it is a manager

inline int isFull(pc_queue_t *queue) {
    return queue->pcq_head == queue->pcq_tail && queue->pcq_current_size == queue->pcq_capacity;
}

inline int isEmpty(pc_queue_t *queue) {
    return queue->pcq_head == queue->pcq_tail && queue->pcq_current_size == 0;
}

// pcq_create: create a queue, with a given (fixed) capacity
//
// Memory: the queue pointer must be previously allocated
// (either on the stack or the heap)
int pcq_create(pc_queue_t *queue, size_t capacity) {

    queue->pcq_buffer = malloc(capacity * sizeof(void*));
    for (int i=0; i<capacity; ++i) {
        queue->pcq_buffer[i] = malloc(capacity * sizeof(void));
    }

    if(queue)
    {                                    // initialize
        queue->pcq_capacity = capacity; 
        queue->pcq_head = 0;
        queue->pcq_tail = 0;
        queue->pcq_current_size = 0;
        return 0;
    }
    return -1;
    
}

// pcq_destroy: releases the internal resources of the queue
//
// Memory: does not free the queue pointer itself
int pcq_destroy(pc_queue_t *queue) {

    for (int i=0; i<queue->pcq_capacity; ++i) {
        free(queue->pcq_buffer[i]);
    }

    free(queue->pcq_buffer);

    return 0;
}

// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem) {
   
    pthread_mutex_lock(&queue->pcq_popper_condvar_lock);
    while(isFull(queue)) {
        pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock);
    }
    pthread_mutex_unlock(&queue->pcq_popper_condvar_lock);
    
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size++;
    pthread_mutex_unlock(&queue->pcq_current_size_lock);


    pthread_mutex_lock(&queue->pcq_head_lock);
    strcpy(queue->pcq_buffer[queue->pcq_head],elem);
    
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;


    pthread_mutex_unlock(&queue->pcq_head_lock);
    pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);
    pthread_cond_signal(&queue->pcq_pusher_condvar);
    pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);

    return 0;
}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue) {

    pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);
    while(isEmpty(queue)) {
        pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
    }
    pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);
    
    pthread_mutex_lock(&queue->pcq_tail_lock);

    void *elem;
    elem = queue->pcq_buffer[queue->pcq_tail];

    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    pthread_mutex_unlock(&queue->pcq_tail_lock);
    
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    queue->pcq_current_size--;
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    pthread_mutex_lock(&queue->pcq_popper_condvar_lock);
    pthread_cond_signal(&queue->pcq_popper_condvar);
    pthread_mutex_unlock(&queue->pcq_popper_condvar_lock);

    return elem;
    
}