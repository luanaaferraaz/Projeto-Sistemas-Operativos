#ifndef AUX_H
#define AUX_H
#include <stdbool.h>
/*
* aux contains functions that perform things that were not asked to do
* but we decided to implement in order to make our program more efficient
*/



 /*
 * send_connected_message & write_connected_message
 * Write a message on a pipe
 * send_connected_message:  
 * Input:
 *   - client_name: absolute path to the pipe for this client
 *   - connected: bool which when: 
 *        -true means the client successfully connected to the box in its request
 *        -false means it failed to connect to the box   
 * write_connected_message:
 * Input:
 *   - pipe: opened pipe ready to write in
 *   - message: message to write in the pipe
 * Returns -1 if an error occured closing the pipe, 0 otherwise                     
 */

int write_connected_message(int pipe, char *message);

void send_connected_msg(char *client_name, bool connected);

/*
 * Reads a message from a pipe.
 *
 * Input:
 *   - client_name: absolute path to the pipe for this client
 * Returns: 0 if the client successfully conected to a box
 *          -1 otherwise                  
 */
int check_connected(char *pipe_name);

#endif