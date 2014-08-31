#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include "process.h"
#include "common.h"
#include "common.pb-c.h"
#include "cuda.h"
#include "list.h"

// cuGetErrorName() doesn't exist for CUDA < 6.0 ...
#if defined(CUDA_VERSION) && CUDA_VERSION < 6000
#include "cuda_errors.h"

CUresult cuGetErrorName(CUresult error, const char** pStr) {
	if (error > CUDA_RESULT_STRING_ARR_SIZE) {
		*pStr = NULL;
		return CUDA_ERROR_INVALID_VALUE;
	}

	*pStr = CUDA_RESULT_STRING[error];
	if (*pStr == NULL)
		return CUDA_ERROR_INVALID_VALUE;
		
	return CUDA_SUCCESS;
}
#endif

CUresult cuda_err_print(CUresult result, int exit_flag) {
	const char *cuda_err_str = NULL;
	
	if (result != CUDA_SUCCESS) {
		cuGetErrorName(result, &cuda_err_str);
		fprintf(stderr, "-\nCUDA Driver API error: %04d - %s [file <%s>, line %i]\n-\n",
				result, cuda_err_str, __FILE__, __LINE__);
	
		if (exit_flag != 0)
			exit(EXIT_FAILURE);
	}

	return result;
}

size_t read_cuda_module_file(void **buffer, const char *filename) {
	size_t b_read=0, buf_size;
	int i = 1;
	FILE *fd;
	struct stat st;
	void *buf = NULL;

	printf("Reading from file <%s> ... ", filename); 
	fd = fopen(filename, "rb");
	if (fd == NULL) {
		perror("fopen failed");
		exit(EXIT_FAILURE);
	}
	

	if (fstat(fileno(fd), &st) != 0) {
		fprintf(stderr, "Reading file size failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	} else if ((!S_ISREG(st.st_mode)) && (!S_ISLNK(st.st_mode))) {
		fprintf(stderr, "Getting file size failed: Not a regular file or symbolic link\n");
		exit(EXIT_FAILURE);
	}

	buf = malloc(st.st_size);
	if (buf == NULL) {
		fprintf(stderr, "buf memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	b_read = fread(buf, 1, st.st_size, fd);
	if (b_read == 0 && ferror(fd) != 0) {
		fprintf(stderr, "fread failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);

	}
	printf("read: %zu ... ", b_read);

	if (b_read != st.st_size) {
		fprintf(stderr, "Reading file failed: read %zu vs %jd expected\n", b_read, st.st_size);
		exit(EXIT_FAILURE);
	}
	
	*buffer = buf;

	fclose(fd);
	printf("Done\n");

	return b_read;
}

void print_file_as_hex(uint8_t *file, size_t file_size) {
	int i;
	
	printf("File size: %zu\n", file_size);
	for (i = 0; i < file_size; i++) {
		if (i % 14 == 0)
			printf("\n");
		printf("%02X ", (char) file[i]);
	}
	printf("\n");
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
	new_node->cuda_dev_handle = NULL;
	new_node->cuda_ctx_handle = NULL;
	new_node->cuda_dev_handle = NULL;
	new_node->cuda_fun_handle = NULL;

	printf("Adding client <%d> to list\n", client_id);
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

int update_device_of_client(uintptr_t *dev_ptr, cuda_device_node *free_list, int dev_ordinal, client_node *client) {
	cuda_device_node *tmp;
	int i = 0;

	// TODO: support more than one devices per client.
	printf("Updating device of client <%d> ... ", client->id);

	tmp = list_first_entry_or_null(&free_list->node, cuda_device_node, node);
	if (tmp == NULL) {
		fprintf(stderr, "No CUDA devices available for assignment\n");
		return -1;
	}
	while (i++ < dev_ordinal) {
		tmp = list_next_entry(tmp, node);
		if (&tmp->node == &free_list->node) {
			fprintf(stderr, "No CUDA devices available for assignment with the desired ordinal\n");
			return -1;
		}
	}

	client->cuda_dev_node = tmp;
	*dev_ptr = (uintptr_t) tmp->cuda_device;

	printf("updated to <%s>@%p\n", tmp->cuda_device_name, tmp->cuda_device);

	return 0;
}

int assign_device_to_client(cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *device_n;
	int i = 0;

	device_n = client->cuda_dev_node;
	printf("Assigning device <%s>@%p to client <%d> ...\n", device_n->cuda_device_name, device_n->cuda_device, client->id);

	if (device_n->is_busy == 1) {
		fprintf(stderr, "Requested CUDA device is busy\n");
		return -1;
	}


	printf("Moving device <%s>@%p to busy list ... ", device_n->cuda_device_name, device_n->cuda_device);
	device_n->is_busy = 1;
	list_move_tail(&device_n->node, &busy_list->node);

	client->cuda_dev_handle = device_n->cuda_device;
	printf("Done\n");	

	return 0;
}

int free_device_from_client(cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *device_n;

	device_n = client->cuda_dev_node;
	printf("Freeing device <%s>@%p from client <%d> ... ", device_n->cuda_device_name, device_n->cuda_device, client->id);

	list_move_tail(&device_n->node, &free_list->node);
	device_n->is_busy = 0;
	printf("Done\n");

	return 0;
}

int create_context_of_client(uintptr_t *ctx_ptr, unsigned int flags, client_node *client) {
	CUcontext *cuda_context;
	CUresult res = 0;

	// TODO: support more than one contexts per client.
	printf("Creating CUDA context of client <%d> ... ", client->id);

	res = cuda_err_print(cuCtxCreate(cuda_context, flags, *(client->cuda_dev_handle)), 0);

	if (res == CUDA_SUCCESS) {
		client->cuda_ctx_handle = cuda_context;
		*ctx_ptr = (uintptr_t) cuda_context;
	}

	printf("created @%p ... Done\n", cuda_context);

	return res;
}

int destroy_context_of_client(client_node *client) {
	CUresult res = 0;

	// TODO: free modules/functions allocated handles (?)	
	printf("Destroying CUDA context of client <%d> ... ", client->id);
	
	if (client->cuda_ctx_handle != NULL) 
		res = cuda_err_print(cuCtxDestroy(*(client->cuda_ctx_handle)), 0);

	if (res == CUDA_SUCCESS)
		client->cuda_ctx_handle = NULL;

	printf("client handle now @%p ... Done\n", client->cuda_ctx_handle);

	return res;
}

int load_module_of_client(uintptr_t *mod_ptr, ProtobufCBinaryData *image, client_node *client) {
	CUresult res;
	CUmodule *cuda_module;

	// TODO: support more than one modules per client.	
	printf("Loading CUDA module of client <%d> ... ", client->id);

	cuda_module = malloc(sizeof(*cuda_module));
	if (cuda_module == NULL) {
		fprintf(stderr, "cuda_module memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	res = cuda_err_print(cuModuleLoadData(cuda_module, image->data), 0);

	if (res == CUDA_SUCCESS) {
		client->cuda_mod_handle = cuda_module;
		*mod_ptr = (uintptr_t) cuda_module;
	}

	return res;
}

int get_module_function_of_client(uintptr_t *fun_ptr, char *func_name, client_node *client) {
	CUresult res;
	CUfunction *cuda_func;

	// TODO: support more than one functions per client.	
	printf("Loading CUDA module function of client <%d> ... ", client->id);

	cuda_func = malloc(sizeof(*cuda_func));
	if (cuda_func == NULL) {
		fprintf(stderr, "cuda_func memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	res = cuda_err_print(cuModuleGetFunction(cuda_func,
				*(client->cuda_mod_handle), func_name), 0);

	if (res == CUDA_SUCCESS) {
		client->cuda_fun_handle = cuda_func;
		*fun_ptr = (uintptr_t) cuda_func;
	}

	return res;
}

int memory_allocate_for_client(uintptr_t *dev_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr;
	
	printf("Allocating CUDA device memory of size %zuB...\n", mem_size);
	cuda_dev_ptr = malloc(sizeof(*cuda_dev_ptr));
	if (cuda_dev_ptr == NULL) {
		fprintf(stderr, "cuda_dev_ptr memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	
	res = cuda_err_print(cuMemAlloc(cuda_dev_ptr, mem_size), 0);
	if (res == CUDA_SUCCESS) {
		*dev_mem_ptr = (uintptr_t) cuda_dev_ptr;
	}

	return res;
}

int memory_free_for_client(uintptr_t dev_mem_ptr) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr = (CUdeviceptr *) dev_mem_ptr;

	printf("Freeing CUDA device memory @%p...\n", cuda_dev_ptr);

	res = cuda_err_print(cuMemFree(*cuda_dev_ptr), 0);

	return res;
}

int memcpy_host_to_dev_for_client(uintptr_t dev_mem_ptr, void *host_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr = (CUdeviceptr *) dev_mem_ptr;

	printf("Memcpying %zuB from host to CUDA device @%p...\n", mem_size, cuda_dev_ptr);

	res = cuda_err_print(cuMemcpyHtoD(*cuda_dev_ptr, host_mem_ptr, mem_size), 0);

	return res;
}

int memcpy_dev_to_host_for_client(void **host_mem_ptr, size_t *host_mem_size, uintptr_t dev_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr = (CUdeviceptr *) dev_mem_ptr;

	printf("Memcpying %zuB from CUDA device @%p to host...\n", mem_size, cuda_dev_ptr);

	*host_mem_size = mem_size;
	*host_mem_ptr = malloc(mem_size);
	if (*host_mem_ptr == NULL) {
		fprintf(stderr, "host_mem_ptr memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	res = cuda_err_print(cuMemcpyDtoH(*host_mem_ptr, *cuda_dev_ptr, mem_size), 0);

	return res;
}

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_handle) {
	CUresult cuda_result = 0;
	CudaCmd *cmd = cmd_ptr;
	uintptr_t id_ptr = 0;
	void *extra_args = NULL, *res_data = NULL;
	size_t extra_args_size = 0, res_length = 0;
	var *res = NULL;
	var_type res_type;

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
			printf("cmd->int_args[0] = %d\n", cmd->int_args[0]);
			if(update_device_of_client(&id_ptr, free_list, cmd->int_args[0], client_handle) < 0)
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
			cuda_result = create_context_of_client(&id_ptr, cmd->uint_args[0], client_handle);

			break;
		case CONTEXT_DESTROY:
			printf("Executing cuCtxDestroy...\n");
			cuda_result = destroy_context_of_client(client_handle);
			free_device_from_client(free_list, busy_list, client_handle);
			break;
		case MODULE_LOAD:
			printf("Executing cuModuleLoad...\n");
			//print_file_as_hex(cmd->extra_args[0].data, cmd->extra_args[0].len);
			cuda_result = load_module_of_client(&id_ptr, &(cmd->extra_args[0]), client_handle);
			break;
		case MODULE_GET_FUNCTION:
			printf("Executing cuModuleGetFuction...\n");
			cuda_result = get_module_function_of_client(&id_ptr, cmd->str_args[0], client_handle);
			break;
		case MEMORY_ALLOCATE:
			printf("Executing cuMemAlloc...\n");
			cuda_result = memory_allocate_for_client(&id_ptr, cmd->uint_args[0]);
			break;
		case MEMORY_FREE:
			printf("Executing cuMemFree...\n");
			cuda_result = memory_free_for_client(cmd->uint_args[0]);
			break;
		case MEMCPY_HOST_TO_DEV:
			printf("Executing cuMemcpyHtoD...\n");
			cuda_result = memcpy_host_to_dev_for_client(cmd->uint_args[0], cmd->extra_args[0].data, cmd->extra_args[0].len);
			break;
		case MEMCPY_DEV_TO_HOST:
			printf("Executing cuMemcpyDtoH...\n");
			cuda_result = memcpy_dev_to_host_for_client(&extra_args, &extra_args_size, cmd->uint_args[0], cmd->uint_args[1]);
			break;
	}

	if (id_ptr != 0) {
		res_type = UINT;
		res_length = sizeof(uintptr_t);
		res_data = &id_ptr;
	} else if (extra_args_size != 0) {	
		res_type = BYTES;
		res_length = extra_args_size;
		res_data = extra_args;
	}

	if (res_length > 0) {
		res = malloc(sizeof(*res));
		if (res == NULL) {
			fprintf(stderr, "res memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
		res->type = res_type;
		res->length = res_length;
		res->data = malloc(res_length);
		if (res->data == NULL) {
			fprintf(stderr, "res->data memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
		memcpy(res->data, res_data, res_length);
	}
	*result = res;

	if (extra_args != NULL)
		free(extra_args);

	return cuda_result;
}

int process_cuda_device_query(void **result, void *free_list, void *busy_list) {
	CudaDeviceList *cuda_devs;
	CudaDevice **cuda_devs_dev;
	CUresult res = CUDA_SUCCESS;
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

int pack_cuda_cmd_result(void **payload, void *result, int res_code) {
	var *res = result;
	CudaCmd *cmd;

	printf("Packing CUDA cmd result...\n");
	
	cmd = malloc(sizeof(CudaCmd));
	if (cmd == NULL) {
		fprintf(stderr, "cmd memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cuda_cmd__init(cmd);

	cmd->type = RESULT;
	if (res != NULL) {
		cmd->arg_count = 2;
		switch (res->type) {
			case UINT:
				cmd->n_uint_args = 1;
				cmd->uint_args = malloc(sizeof(*(cmd->uint_args)) * cmd->n_uint_args);
				if (cmd->uint_args == NULL) {
					fprintf(stderr, "cmd->uint_args memory allocation failed\n");
					exit(EXIT_FAILURE);
				}
				cmd->uint_args = res->data;
				printf("result: %p\n", (void *)cmd->uint_args[0]);
				break;
			case BYTES:
				cmd->n_extra_args = 1;
				cmd->extra_args = malloc(sizeof(*(cmd->extra_args)) * cmd->n_extra_args);
				if (cmd->extra_args == NULL) {
					fprintf(stderr, "cmd->extra_args memory allocation failed\n");
					exit(EXIT_FAILURE);
				}
				cmd->extra_args[0].data = res->data;
				cmd->extra_args[0].len = res->length;
				break;
		}
	} else {
		cmd->arg_count = 1;
	}
	cmd->n_int_args = 1;
	cmd->int_args = malloc(sizeof(*(cmd->int_args)) * cmd->n_int_args);
	if (cmd->int_args == NULL) {
		fprintf(stderr, "cmd->int_args memory allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd->int_args[0] = res_code;

	printf("res_code: %" PRId64 "\n", cmd->int_args[0]);

	*payload = cmd;
	return 0;
}
