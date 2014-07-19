#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "process.h"
#include "common.h"
#include "common.pb-c.h"
#include "cuda.h"
#include "list.h"

// cuGetErrorName() doesn't exist for CUDA < 5.5 ...
#if defined(CUDA_VERSION) && CUDA_VERSION < 6000
CUresult cuGetErrorName(CUresult error, const char** pStr) {
	const char *a = "no error name";
	*pStr = a;

	return CUDA_SUCCESS;
}
#endif

CUresult cuda_err_print(CUresult result, int exit_flag) {
	const char **cuda_err_str = NULL;
	
	if (result != CUDA_SUCCESS) {
		cuGetErrorName(result, cuda_err_str);
		fprintf(stderr, "CUDA Driver API error: %04d - %s [file <%s>, line %i]\n",
				result, *cuda_err_str, __FILE__, __LINE__);
	
		if (exit_flag != 0)
			exit(EXIT_FAILURE);
	}

	return result;
}

int add_device_to_list(cuda_device_node *dev_list, int dev_id) {
		cuda_device_node *cuda_dev_node;
		char cuda_dev_name[CUDA_DEV_NAME_MAX];
		CUdevice *cuda_device;
	   
		cuda_device = malloc(sizeof(CUdevice));
		if (cuda_device == NULL) {
			fprintf(stderr, "cuda_device memory allocation failed\n");
			exit(EXIT_FAILURE);
		}

		cuda_dev_node = malloc(sizeof(*cuda_dev_node));
		if (cuda_dev_node == NULL) {
			fprintf(stderr, "cuda_dev_node memory allocation failed\n");
			exit(EXIT_FAILURE);
		}

		if (cuda_err_print(cuDeviceGet(cuda_device, dev_id), 0) != CUDA_SUCCESS)
			return -1;

		if (cuda_err_print(cuDeviceGetName(cuda_dev_name, CUDA_DEV_NAME_MAX, *cuda_device), 0) != CUDA_SUCCESS)
			return -1;

		cuda_dev_node->cuda_device = cuda_device;

		memcpy(cuda_dev_node->cuda_device_name, cuda_dev_name, CUDA_DEV_NAME_MAX);

		fprintf(stdout, "Adding device [%d]@%p -> %s\n", dev_id, cuda_dev_node->cuda_device, cuda_dev_node->cuda_device_name);

		list_add_tail(&cuda_dev_node->node, &dev_list->node);

		return 0;
}

int discover_cuda_devices(void **free_list, void **busy_list) {
	int i, cuda_dev_count = 0;
	cuda_device_node *free_cuda_devs, *busy_cuda_devs;

	printf("Discovering available devices...\n");
	cuda_err_print(cuInit(0), 1);

	// Get device count
	cuda_err_print(cuDeviceGetCount(&cuda_dev_count), 0);
	if (cuda_dev_count == 0) {
		fprintf(stderr, "No CUDA DEVICE available\n");
		exit(EXIT_FAILURE);
	}
	printf("Available CUDA devices: %d\n", cuda_dev_count);
	
	// Init free and busy CUDA device lists
	free_cuda_devs = malloc(sizeof(cuda_device_node));
	if (free_cuda_devs == NULL) {
		fprintf(stderr, "free_cuda_devs memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	
	busy_cuda_devs = malloc(sizeof(cuda_device_node));
	if (busy_cuda_devs == NULL) {
		fprintf(stderr, "busy_cuda_devs memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	INIT_LIST_HEAD(&free_cuda_devs->node);
	INIT_LIST_HEAD(&busy_cuda_devs->node);

	// Add available CUDA devices to free_list
	for (i=0; i<cuda_dev_count; i++)
		add_device_to_list(free_cuda_devs, i);

	*free_list = free_cuda_devs;
	*busy_list = busy_cuda_devs;

	return 0;
}

void print_cuda_devices(void *free_list, void *busy_list) {
	cuda_device_node *free_list_p, *busy_list_p, *pos;
	int i = 0;

	printf("\nAvailable CUDA devices:\n");
	free_list_p = free_list;
	busy_list_p = busy_list;

	printf("| Free:\n");
	list_for_each_entry(pos, &free_list_p->node, node) {
		printf("|   [%d] %s\n", i, pos->cuda_device_name);
	}

	printf("| Busy:\n");
	list_for_each_entry(pos, &busy_list_p->node, node){
		printf("|   [%d] %s\n", i, pos->cuda_device_name);
	}
}

int process_cuda_cmd(void **result, void *cmd_ptr) {
	CUresult cuda_result;
	CudaCmd *cmd = cmd_ptr;

	printf("Processing CUDA_CMD\n");
	switch(cmd->type) {
		case INIT:
			printf("Executing cuInit...\n");
			if (cmd->n_uint_args != 1) {
				fprintf(stderr, "Unsufficient arguments for cuInit!\n");
				return -1;
			}
			cuda_result = cuInit(cmd->uint_args[0]);
			break;
		case CONTEXT_CREATE:
			break;
		case CONTEXT_DESTROY:
			break;
	}

	return 0;
}

int process_cuda_device_query(void **result, void **cuda_dev_array, int *cuda_dev_arr_size) {
	CudaDeviceList *cuda_devs;
	CudaDevice **cuda_devs_dev;
	CUresult res, error = CUDA_SUCCESS;
	int i, cuda_dev_count = 0;
	char *cuda_dev_name = NULL;
	CUdevice cuda_device;

	printf("Processing CUDA_DEVICE_QUERY\n");
//#if 0
	if (*cuda_dev_array == NULL) {
		res = cuInit(0); 
		if (res != CUDA_SUCCESS)
			return -1;

		// Get device count
		cuda_err_print(cuDeviceGetCount(&cuda_dev_count), 0);
		if (cuda_dev_count == 0) {
			fprintf(stderr, "No CUDA DEVICE available\n");
			return -1;
		}

		printf("Available CUDA devices: %d\n", cuda_dev_count);

		// Init variables
		cuda_devs = malloc(sizeof(CudaDeviceList));
		if (cuda_devs == NULL) {
			fprintf(stderr, "cuda_devs memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
		cuda_device_list__init(cuda_devs);
		cuda_devs_dev = malloc(sizeof(CudaDevice *) * cuda_dev_count);
		if (cuda_devs_dev == NULL) {
			fprintf(stderr, "cuda_devs_dev memory allocation failed\n");
			exit(EXIT_FAILURE);
		}

		// Add devices
		printf("Adding devices...\n");
		for (i=0; i<cuda_dev_count; i++) {
			if (cuda_err_print(cuDeviceGet(&cuda_device, i), 0) != CUDA_SUCCESS)
				return -1;
			cuda_dev_name = malloc(CUDA_DEV_NAME_MAX);
			if (cuda_err_print(cuDeviceGetName(cuda_dev_name, CUDA_DEV_NAME_MAX, cuda_device), 0) != CUDA_SUCCESS)
				return -1;
			printf("%d -> %s\n", i, cuda_dev_name);
			cuda_devs_dev[i] = malloc(sizeof(CudaDevice));
			if (cuda_devs_dev[i] == NULL) {
				fprintf(stderr, "cuda_devs_dev[%d] memory allocation failed\n", i);
				exit(EXIT_FAILURE);
			}
			cuda_device__init(cuda_devs_dev[i]);
			cuda_devs_dev[i]->is_busy = 0;
			cuda_devs_dev[i]->name = cuda_dev_name;
		}
		cuda_devs->n_device = cuda_dev_count;
		cuda_devs->devices_free = cuda_dev_count;
		cuda_devs->device = cuda_devs_dev;
		*cuda_dev_array = cuda_devs;
		*cuda_dev_arr_size = cuda_dev_count;
	}
//#endif



	*result = *cuda_dev_array;
	return 0;
}
