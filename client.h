#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

void get_server_connection(int *client_sock_fd, struct addrinfo *server_addr);

int64_t get_cuda_cmd_result(void **result, int sock_fd);

int send_cuda_cmd(int sock_fd, var **args, size_t arg_count, int type); 

#endif /* CLIENT_H */
