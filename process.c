#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "process.h"
#include "common.h"
#include "common.pb-c.h"
#include "cuda.h"
#include "list.h"

// cuGetErrorName() doesn't exist for CUDA < 6.0 ...
#if defined(CUDA_VERSION) && CUDA_VERSION < 6000
CUresult cuGetErrorName(CUresult error, const char** pStr) {
	char *a;
	a = malloc(sizeof(char) * (strlen("<no error name>")+1));
	strcpy(a, "<no error name>");
	*pStr = a;

	return CUDA_SUCCESS;
}
#endif

CUresult cuda_err_print(CUresult result, int exit_flag) {
	const char *cuda_err_str = NULL;
	
	if (result != CUDA_SUCCESS) {
		cuGetErrorName(result, &cuda_err_str);
		fprintf(stderr, "CUDA Driver API error: %04d - %s [file <%s>, line %i]\n",
				result, cuda_err_str, __FILE__, __LINE__);
	
		if (exit_flag != 0)
			exit(EXIT_FAILURE);
	}

	return result;
}

void init_device_list(cuda_device_node **list) {
	cuda_device_node *empty_list;

	empty_list = malloc(sizeof(cuda_device_node));
	if (empty_list == NULL) {
		fprintf(stderr, "local_list memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	INIT_LIST_HEAD(&empty_list->node);
	
	*list = empty_list;
}

void init_client_list(client_node **list) {
	client_node *empty_list;

	empty_list = malloc(sizeof(client_node));
	if (empty_list == NULL) {
		fprintf(stderr, "local_list memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	INIT_LIST_HEAD(&empty_list->node);
	
	*list = empty_list;
}

void free_cdn_list(void *list) {
	cuda_device_node *pos, *tmp, *cdn_list;
	int i = 0;
	cdn_list = list;

	printf("Freeing list... ");
	list_for_each_entry_safe(pos, tmp, &cdn_list->node, node) {
		list_del(&pos->node);
		free(pos);
		i++;
	}
	printf("%d nodes freed\n", i);
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
		cuda_dev_node->is_busy = 0;

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
	init_device_list(&free_cuda_devs);
	init_device_list(&busy_cuda_devs);

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
		printf("|   [%d] %s\n", i++, pos->cuda_device_name);
	}

	printf("| Busy:\n");
	i = 0;
	list_for_each_entry(pos, &busy_list_p->node, node){
		printf("|   [%d] %s\n", i++, pos->cuda_device_name);
	}
}

int add_client_to_list(void **client_handle, void **client_list, int client_id) {
	client_node *client_list_p=*client_list, *new_node, *pos;

	if (client_list_p == NULL)
		init_client_list(&client_list_p);
	else {
		// check if client exists in list
		list_for_each_entry(pos, &client_list_p->node, node) {
			if (pos->id == client_id) {
				printf("Client is already in the list\n");
				*client_handle = pos;
				return 0;
			}
		}
	}

	new_node = malloc(sizeof(*new_node));
	if (new_node == NULL) {
		fprintf(stderr, "new_node memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	new_node->id = client_id;

	fprintf(stdout, "Adding client <%d> to list\n", client_id);
	list_add_tail(&new_node->node, &client_list_p->node);

	*client_list = client_list_p;
	*client_handle = new_node;
	return 0;
}

void print_clients(void *client_list) {
	client_node *client_list_p=client_list, *pos;
	int i = 0;

	printf("\nClients:\n");
	list_for_each_entry(pos, &client_list_p->node, node) {
		printf("| [%d] <%d>\n", i++, pos->id);
	}
}

int update_device_of_client(cuda_device_node *free_list, int dev_ordinal, client_node *client) {
	cuda_device_node *tmp;
	int i = 0;

	tmp = list_first_entry_or_null(&free_list->node, cuda_device_node, node);
	if (tmp == NULL) {
		printf("No CUDA devices available for assignment\n");
		return -1;
	}
	while (i++ < dev_ordinal) {
		tmp = list_next_entry(tmp, node);
		if (&tmp->node == &free_list->node) {
			printf("No CUDA devices available for assignment with the desired ordinal\n");
			return -1;
		}
	}

	client->cuda_dev_node = tmp;
	return 0;
}

int assign_device_to_client(cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *device_node;
	int i = 0;

	device_node = client->cuda_dev_node;
	if (device_node->is_busy == 1) {
		fprintf(stderr, "Requested CUDA device is busy\n");
		return -1;
	}


	printf("Moving device <%s>@%p to busy list\n", device_node->cuda_device_name, device_node->cuda_device);
	device_node->is_busy = 1;
	list_move_tail(&device_node->node, &busy_list->node);

	client->cuda_dev_handle = device_node->cuda_device;
	return 0;
}

int create_context_of_client(unsigned int flags, client_node *client) {
	CUcontext *cuda_context;
	CUresult res = 0;

	res = cuda_err_print(cuCtxCreate(cuda_context, flags, *(client->cuda_dev_handle)), 0);

	if (res == CUDA_SUCCESS)
		client->cuda_ctx_handle = cuda_context;

	return res;
}

int destroy_context_of_client(client_node *client) {
	CUresult res = 0;
	
	if (client->cuda_ctx_handle != NULL) 
		res = cuda_err_print(cuCtxDestroy(*(client->cuda_ctx_handle)), 0);

	if (res == CUDA_SUCCESS)
		client->cuda_ctx_handle = NULL;

	return res;
}

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_handle) {
	CUresult cuda_result = 0;
	CudaCmd *cmd = cmd_ptr;

	if (client_handle == NULL) {
		fprintf(stderr, "process_cuda_cmd: Invalid client handler\n");
		return -1;
	}

	printf("Processing CUDA_CMD\n");
	switch(cmd->type) {
		case INIT:
			printf("Executing cuInit...\n");
			
			// cuInit() should have already been executed by the server 
			// by that point, but running anyway (for now)...
			cuda_result = cuda_err_print(cuInit(cmd->uint_args[0]), 0);
			break;
		case DEVICE_GET:
			printf("Executing cuDeviceGet...\n");
			if(update_device_of_client(free_list,cmd->int_args[0], client_handle) < 0)
				cuda_result = CUDA_ERROR_INVALID_DEVICE;
			else
				cuda_result = CUDA_SUCCESS;

			break;
		case CONTEXT_CREATE:
			printf("Executing cuCtxCreate...\n");
			if(assign_device_to_client(free_list, busy_list, client_handle) < 0) {
				// TODO: Device was busy, give appropriate response to client.
				cuda_result = -2;
				break;
			}
			cuda_result = create_context_of_client(cmd->uint_args[0], client_handle);

			break;
		case CONTEXT_DESTROY:
			printf("Executing cuCtxDestroy...\n");
			cuda_result = destroy_context_of_client(client_handle);
			break;
	}

	return cuda_result;
}

int process_cuda_device_query(void **result, void *free_list, void *busy_list) {
	CudaDeviceList *cuda_devs;
	CudaDevice **cuda_devs_dev;
	CUresult res, error = CUDA_SUCCESS;
	int i, cuda_dev_count = 0;
	cuda_device_node *pos, *free_list_p=free_list, *busy_list_p=busy_list;

	printf("Processing CUDA_DEVICE_QUERY\n");
	list_for_each_entry(pos, &free_list_p->node, node) {
		cuda_dev_count++;
	}
	list_for_each_entry(pos, &busy_list_p->node, node){
		cuda_dev_count++;
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
	// free
	i = 0;
	list_for_each_entry(pos, &free_list_p->node, node) {
		printf("%d -> %s\n", i, pos->cuda_device_name);
		cuda_devs_dev[i] = malloc(sizeof(CudaDevice));
		if (cuda_devs_dev[i] == NULL) {
			fprintf(stderr, "cuda_devs_dev[%d] memory allocation failed\n", i);
			exit(EXIT_FAILURE);
		}
		cuda_device__init(cuda_devs_dev[i]);
		cuda_devs_dev[i]->is_busy = 0;
		cuda_devs_dev[i]->name = pos->cuda_device_name;
		i++;
	}
	cuda_devs->devices_free = i;
	
	// busy
	i = 0;
	list_for_each_entry(pos, &busy_list_p->node, node){
		printf("%d -> %s\n", i, pos->cuda_device_name);
		cuda_devs_dev[i] = malloc(sizeof(CudaDevice));
		if (cuda_devs_dev[i] == NULL) {
			fprintf(stderr, "cuda_devs_dev[%d] memory allocation failed\n", i);
			exit(EXIT_FAILURE);
		}
		cuda_device__init(cuda_devs_dev[i]);
		cuda_devs_dev[i]->is_busy = 1;
		cuda_devs_dev[i]->name = pos->cuda_device_name;
		i++;
	}
	
	cuda_devs->n_device = cuda_dev_count;
	cuda_devs->device = cuda_devs_dev;
	*result = cuda_devs;

	return 0;
}
