#ifndef PROCESS_H
#define PROCESS_H

#include <cuda.h>
#include "list.h"

#define CUDA_DEV_NAME_MAX 100
typedef struct cuda_device_node_s {
	CUdevice *cuda_device;
	char cuda_device_name[CUDA_DEV_NAME_MAX];
	struct list_head node;
} cuda_device_node;

typedef struct client_node_s {
	int id;
	struct list_head node;
	CUdevice *cuda_dev_handle;
	CUcontext *cuda_ctx_handle;
} client_node;


int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_list);

int process_cuda_device_query(void **result, void *free_list, void *busy_list);

int discover_cuda_devices(void **free_list, void **busy_list);

void print_cuda_devices(void *free_list, void *busy_list);

int add_client_to_list(void **client_handle, void **client_list, int client_id); 

void print_clients(void *client_list); 

void free_cdn_list(void *list);

#endif /* PROCESS_H */
