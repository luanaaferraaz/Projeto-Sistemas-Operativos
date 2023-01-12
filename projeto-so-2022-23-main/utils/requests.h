#ifndef REQUESTS_H
#define REQUESTS_H

void send_error(int pipe, char *code, char *return_code, char *error_message);
void send_message(char *message, char *box_name, char *pipe_name);
void send_msg_request(int pipe, char *code, char *client_pipe_name, char* box_name);
void send_msg_request_list(int pipe, char *code, char *client_pipe_name);
void send_request(int code, char* register_pipe_name, char *client_pipe_name, char *box_name);
void send_request_list(char* register_pipe_name, char *client_pipe_name);
void send_response(char* code, char* client_name, int return_code, char* error_message);

#endif // REQUESTS_H