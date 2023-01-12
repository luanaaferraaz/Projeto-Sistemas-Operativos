#include "producer-consumer.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define SIZE 32


int isFull(pc_queue_t *queue) {
    if ((queue->pcq_head == queue->pcq_tail + 1) || 
    (queue->pcq_head == 1 && queue->pcq_tail == SIZE)) {
        return 1;
    }
    return 0;
}

int isEmpty(pc_queue_t *queue) {
    if (queue->pcq_head == 0) {
        return 1;
    }
    return 0;
}

// pcq_create: create a queue, with a given (fixed) capacity
//
// Memory: the queue pointer must be previously allocated
// (either on the stack or the heap)
int pcq_create(pc_queue_t *queue, size_t capacity) {

    //queue = malloc(sizeof(*queue) * capacity);

    queue->pcq_buffer = malloc(capacity * sizeof(void*));
    for (int i=0; i<capacity; ++i) {
        queue->pcq_buffer[i] = malloc(capacity * sizeof(void));
    }

    if(queue)
    {
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

    //free(queue);

    return 0;
}

// pcq_enqueue: insert a new element at the front of the queue
//
// If the queue is full, sleep until the queue has space
int pcq_enqueue(pc_queue_t *queue, void *elem) {
   
    while(true) {
        if(isFull(queue) == 0) {
            break;
        }
    }
    
    if (queue->pcq_head == 0) {
        queue->pcq_head = 1;
    }
    queue->pcq_head = (queue->pcq_head) % SIZE;
    queue->pcq_buffer[queue->pcq_head] = elem;
    queue->pcq_current_size++;
    return 0;
}

// pcq_dequeue: remove an element from the back of the queue
//
// If the queue is empty, sleep until the queue has an element
void *pcq_dequeue(pc_queue_t *queue) {

    while(true) {
        if(isEmpty(queue) == 1) {
            break;
        }
    }

    if (queue->pcq_head == queue->pcq_tail) { // has only one element
        queue->pcq_head = 0;
        queue->pcq_tail = 0;
    }  
    else {
        queue->pcq_tail = (queue->pcq_tail) % SIZE;
    }
    queue->pcq_current_size--;
    return 0;
    
}