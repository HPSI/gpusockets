#ifndef PROCESS_H
#define PROCESS_H

#include <cuda.h>
#include "list.h"

#define CUDA_DEV_NAME_MAX 100
typedef struct cuda_device_node_s {
	CUdevice *cuda_device;
	struct list_head node;
	char cuda_device_name[CUDA_DEV_NAME_MAX];
	int is_busy;
} cuda_device_node;

typedef struct client_node_s {
	int id;
	struct list_head node;
	cuda_device_node *cuda_dev_node;
	CUdevice *cuda_dev_handle;
	CUcontext *cuda_ctx_handle;
	CUmodule *cuda_mod_handle;
	CUfunction *cuda_fun_handle;
} client_node;


size_t read_cuda_module_file(void **buffer, const char *filename);

int discover_cuda_devices(void **free_list, void **busy_list);

void print_cuda_devices(void *free_list, void *busy_list);

int add_client_to_list(void **client_handle, void **client_list, int client_id); 

void print_clients(void *client_list); 

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_list);

int process_cuda_device_query(void **result, void *free_list, void *busy_list);

void free_cdn_list(void *list);

int pack_cuda_cmd_result(void **payload, void *result, int res_code);

#endif /* PROCESS_H */
