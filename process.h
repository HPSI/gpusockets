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

int process_cuda_cmd(void **result, void *cmd_ptr);

int process_cuda_device_query(void **result, void **cuda_dev_array, int *cuda_dev_arr_size);

int discover_cuda_devices(void **free_list, void **busy_list);

void print_cuda_devices(void *free_list, void *busy_list);

#endif /* PROCESS_H */
