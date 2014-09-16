#ifndef PROCESS_H
#define PROCESS_H

#include <cuda.h>
#include "list.h"
#include "common.h"

#define CUDA_DEV_NAME_MAX 100
typedef struct cuda_device_node_s {
	CUdevice *cuda_device;
	struct list_head node;
	char cuda_device_name[CUDA_DEV_NAME_MAX];
	int is_busy;
} cuda_device_node;

typedef struct param_node_s {
	struct list_head node;
	int id;
	uint64_t ptr;
} param_node;

typedef struct client_node_s {
	int id;
	int dev_count;
	struct list_head node;
	param_node *cuda_dev_node;
} client_node;


size_t read_cuda_module_file(void **buffer, const char *filename);

int discover_cuda_devices(void **free_list, void **busy_list);

void print_cuda_devices(void *free_list, void *busy_list);

int add_client_to_list(void **client_handle, void **client_list, int client_id);


void print_clients(void *client_list); 

uint32_t add_param_to_list(param_node **list, uint64_t uintptr);

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_list);

int process_cuda_device_query(void **result, void *free_list, void *busy_list);

void free_cdn_list(void *list);

int pack_cuda_cmd(void **payload, var **args, size_t arg_count, int type); 

#endif /* PROCESS_H */
