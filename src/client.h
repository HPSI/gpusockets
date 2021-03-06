#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "process.h"

typedef struct params_s {
	int id;
	int sock_fd;
	struct addrinfo addr;
	param_node *device;
	param_node *context;
	param_node *module;
	param_node *function;
	param_node *variable;
	param_node *stream;
} params;


int init_client(const char *s_ip, const char *s_port, struct addrinfo *s_addr);

void init_params(params *p);

uint64_t get_param_from_list(param_node *list, uint32_t param_id);

int remove_param_from_list(param_node *list, uint32_t param_id);

void get_server_connection(params *p);

int64_t get_cuda_cmd_result(void **result, int sock_fd);

int send_cuda_cmd(int sock_fd, var **args, size_t arg_count, int type); 

#endif /* CLIENT_H */
