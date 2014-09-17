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

#define cuda_err_print(res, ef) \
	cuda_error_print(res, ef, __FILE__, __LINE__)

inline CUresult cuda_error_print(CUresult result, int exit_flag, const char *file, const int line) {
	const char *cuda_err_str = NULL;
	
	if (result != CUDA_SUCCESS) {
		cuGetErrorName(result, &cuda_err_str);
		fprintf(stderr, "-\nCUDA Driver API error: %04d - %s [%s, %i]\n-\n",
				result, cuda_err_str, file, line);
	
		if (exit_flag != 0)
			exit(EXIT_FAILURE);
	}

	return result;
}

size_t read_cuda_module_file(void **buffer, const char *filename) {
	size_t b_read = 0;
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

	buf = malloc_safe(st.st_size);

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

	empty_list = malloc_safe(sizeof(cuda_device_node));

	INIT_LIST_HEAD(&empty_list->node);
	
	*list = empty_list;
}

void init_client_list(client_node **list) {
	client_node *empty_list;

	empty_list = malloc_safe(sizeof(client_node));

	INIT_LIST_HEAD(&empty_list->node);
	
	*list = empty_list;
}

void init_param_list(param_node **list) {
	param_node *empty_list;

	empty_list = malloc_safe(sizeof(param_node));

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

	cuda_device = malloc_safe(sizeof(CUdevice));

	cuda_dev_node = malloc_safe(sizeof(*cuda_dev_node));

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

	new_node = malloc_safe(sizeof(*new_node));

	new_node->id = client_id;
	new_node->dev_count = 0;
	new_node->cuda_dev_node = NULL;

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

uint32_t add_param_to_list(param_node **list, uint64_t uintptr) {
	uint32_t param_id = 0;
	param_node *new_node, *tmp;
	
	if (*list == NULL)
		init_param_list(list);
	else {
		// TODO: (?) generate unique random id
		tmp = list_last_entry(&((*list)->node), param_node, node);
		param_id = tmp->id + 1;
	}

	new_node = malloc_safe(sizeof(*new_node));
	new_node->id = param_id;
	new_node->ptr = uintptr;

	list_add_tail(&new_node->node, &((*list)->node));

	return param_id;
}

uint64_t get_param_from_list(param_node *list, uint32_t param_id) {
	param_node *pos;
	
	list_for_each_entry(pos, &list->node, node) {
		if (pos->id == param_id)
			return pos->ptr;
	}

	fprintf(stderr, "Requested CUDA device not in client's list!\n");	
	return 0;
}

int remove_param_from_list(param_node *list, uint32_t param_id) {
	param_node *pos, *tmp;
	
	list_for_each_entry_safe(pos, tmp, &list->node, node) {
		if (pos->id == param_id) {
			list_del(&pos->node);
			return 0;
		}
	}

	fprintf(stderr, "Requested CUDA device not in client's list!\n");	
	return -1;
}

int update_device_of_client(uintptr_t *dev_ptr, cuda_device_node *free_list, int dev_ordinal, client_node *client) {
	cuda_device_node *tmp;
	int i = 0, true_ordinal;

	// TODO: support more than one devices per client.
	printf("Updating devices of client <%d>...\n", client->id);

	tmp = list_first_entry_or_null(&free_list->node, cuda_device_node, node);
	if (tmp == NULL) {
		fprintf(stderr, "No CUDA devices available for assignment\n");
		return -1;
	}
	true_ordinal = dev_ordinal - client->dev_count;
	while (i++ < true_ordinal) {
		tmp = list_next_entry(tmp, node);
		if (&tmp->node == &free_list->node) {
			fprintf(stderr, "No CUDA devices available for assignment with the desired ordinal\n");
			return -1;
		}
	}
	
	*dev_ptr = (uintptr_t) tmp->cuda_device;
	// TODO: What if client deviceGets the same ordinal twice?
	add_param_to_list(&client->cuda_dev_node, (uintptr_t) tmp);

	return 0;
}

int assign_device_to_client(uintptr_t dev_ptr, cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *dev_node;
	param_node *pos;
	CUdevice *cuda_device = (CUdevice *) dev_ptr;

	printf("Assigning device @%p to client <%d> ...\n", cuda_device, client->id);

	list_for_each_entry(pos, &client->cuda_dev_node->node, node) {
		dev_node = (cuda_device_node *) pos->ptr;
		if (dev_node->cuda_device == cuda_device) {
			if (dev_node->is_busy == 1) {
				fprintf(stderr, "Requested CUDA device is busy\n");
				return -2;
			}
			printf("Moving device <%s>@%p to busy list\n",
					dev_node->cuda_device_name, dev_node->cuda_device);
			dev_node->is_busy = 1;
			list_move_tail(&dev_node->node, &busy_list->node);
			++client->dev_count;

			return 0;
		}	
	}
	
	fprintf(stderr, "Requested CUDA device not in client's list!\n");	
	return -1;
}

int free_device_from_client(uintptr_t dev_ptr, cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *dev_node;
	param_node *pos;
	CUdevice *cuda_device = (CUdevice *) dev_ptr;
	
	printf("Freeing device @%p from client <%d>...\n", cuda_device, client->id);
	
	list_for_each_entry(pos, &client->cuda_dev_node->node, node) {
		dev_node = (cuda_device_node *) pos->ptr;
		if (dev_node->cuda_device == cuda_device) {
			printf("Moving device <%s>@%p to free list\n",
					dev_node->cuda_device_name, dev_node->cuda_device);
			dev_node->is_busy = 0;
			list_move_tail(&dev_node->node, &free_list->node);
			--client->dev_count;

			return 0;
		}	
	}

	fprintf(stderr, "Requested CUDA device not in client's list!\n");	
	return -1;
}

int create_context_of_client(uintptr_t *ctx_ptr, unsigned int flags, uintptr_t dev_ptr, client_node *client) {
	CUcontext *cuda_context;
	CUdevice *cuda_device = (CUdevice *) dev_ptr;
	CUresult res = 0;

	cuda_context = malloc_safe(sizeof(CUcontext));

	// TODO: support more than one contexts per client.
	printf("Creating CUDA context of client <%d> ... ", client->id);

	res = cuda_err_print(cuCtxCreate(cuda_context, flags, *cuda_device), 0);

	if (res == CUDA_SUCCESS) {
		*ctx_ptr = (uintptr_t) cuda_context;
		printf("created @%p ... Done\n", cuda_context);
	} else {
		printf("failed ... Done\n");
	}

	return res;
}

int destroy_context_of_client(uintptr_t ctx_ptr, client_node *client) {
	CUresult res = 0;
	CUcontext *cuda_context = (CUcontext *) ctx_ptr;

	// TODO: free modules/functions allocated handles (?)	
	printf("Destroying CUDA context @%p of client <%d> ...\n", cuda_context, client->id);
	
	res = cuda_err_print(cuCtxDestroy(*cuda_context), 0);

	return res;
}

int load_module_of_client(uintptr_t *mod_ptr, ProtobufCBinaryData *image, client_node *client) {
	CUresult res;
	CUmodule *cuda_module;

	// TODO: support more than one modules per client.	
	printf("Loading CUDA module of client <%d> ... ", client->id);

	cuda_module = malloc_safe(sizeof(*cuda_module));

	res = cuda_err_print(cuModuleLoadData(cuda_module, image->data), 0);

	if (res == CUDA_SUCCESS)
		*mod_ptr = (uintptr_t) cuda_module;

	return res;
}

int get_module_function_of_client(uintptr_t *fun_ptr, uintptr_t mod_ptr, char *func_name, client_node *client) {
	CUresult res;
	CUfunction *cuda_func;
	CUmodule *cuda_module = (CUmodule *) mod_ptr;

	// TODO: support more than one functions per client.	
	printf("Loading CUDA module function of client <%d> ... ", client->id);

	cuda_func = malloc_safe(sizeof(*cuda_func));

	res = cuda_err_print(cuModuleGetFunction(cuda_func, *cuda_module, func_name), 0);

	if (res == CUDA_SUCCESS)
		*fun_ptr = (uintptr_t) cuda_func;

	return res;
}

int memory_allocate_for_client(uintptr_t *dev_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr;
	
	printf("Allocating CUDA device memory of size %zuB...\n", mem_size);
	cuda_dev_ptr = malloc_safe(sizeof(*cuda_dev_ptr));
	
	res = cuda_err_print(cuMemAlloc(cuda_dev_ptr, mem_size), 0);
	if (res == CUDA_SUCCESS) {
		*dev_mem_ptr = (uintptr_t) cuda_dev_ptr;
		printf("allocated @%p\n", cuda_dev_ptr);
	}

	return res;
}

int memory_free_for_client(uintptr_t dev_mem_ptr) {
	CUresult res;
	CUdeviceptr *cuda_dev_ptr = (CUdeviceptr *) dev_mem_ptr;

	printf("Freeing CUDA device memory @%p...\n", cuda_dev_ptr);

	res = cuda_err_print(cuMemFree(*cuda_dev_ptr), 0);
	if (res == CUDA_SUCCESS)
		free(cuda_dev_ptr);

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
	*host_mem_ptr = malloc_safe(mem_size);

	res = cuda_err_print(cuMemcpyDtoH(*host_mem_ptr, *cuda_dev_ptr, mem_size), 0);

	return res;
}

int launch_kernel_of_client(uint64_t *uints, size_t n_uints, ProtobufCBinaryData *extras, size_t n_extras) {
	CUresult res;
	unsigned int grid_x = uints[0], grid_y = uints[1], grid_z = uints[2],
				 block_x = uints[3], block_y = uints[4], block_z = uints[5],
				 shared_mem_size = uints[6];
	CUfunction *func = (CUfunction *) uints[7];
	CUstream h_stream = (uints[8] != 0) ? *(CUstream *) uints[8] : 0;
	void **params = NULL, **extra = NULL;
	size_t i, n_params = n_uints - 9;

	if (n_params > 0) {
		params = malloc_safe(sizeof(void *) * n_params);
	
		for(i = 0; i < n_params; i++) {
			params[i] = (void *) uints[9 + i]; 
		}
	}
	if (n_extras > 0) {
		extra = malloc_safe(sizeof(void *) * 5);

		extra[0] = CU_LAUNCH_PARAM_BUFFER_POINTER;
		extra[1] = extras[0].data;
		extra[2] = CU_LAUNCH_PARAM_BUFFER_SIZE;
		extra[3] = &(extras[0].len);
		extra[4] = CU_LAUNCH_PARAM_END;
	}

	res = cuda_err_print(cuLaunchKernel(*func, grid_x, grid_y, grid_z,
				block_x, block_y, block_z, shared_mem_size, h_stream,
				params, extra), 0);

	if (params != NULL)
		free(params);

	if (extra != NULL)
		free(extra);

	return res;
}

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, void *client_handle) {
	int cuda_result = 0, arg_count = 0;
	CudaCmd *cmd = cmd_ptr;
	uintptr_t id_ptr = 0;
	void *extra_args = NULL, *res_data = NULL;
	size_t extra_args_size = 0, res_length = 0;
	var **res = NULL;
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
			cuda_result = assign_device_to_client(cmd->uint_args[1], free_list, busy_list, client_handle);
			if (cuda_result	< 0) {
				// TODO: Handle appropriately in client.
				break;
			}
			cuda_result = create_context_of_client(&id_ptr, cmd->uint_args[0], cmd->uint_args[1], client_handle);
			break;
		case CONTEXT_DESTROY:
			printf("Executing cuCtxDestroy...\n");
			cuda_result = destroy_context_of_client(cmd->uint_args[0], client_handle);
			if (cuda_result != CUDA_SUCCESS)
				free_device_from_client(cmd->uint_args[0], free_list, busy_list, client_handle);
			break;
		case MODULE_LOAD:
			printf("Executing cuModuleLoad...\n");
			//print_file_as_hex(cmd->extra_args[0].data, cmd->extra_args[0].len);
			cuda_result = load_module_of_client(&id_ptr, &(cmd->extra_args[0]), client_handle);
			break;
		case MODULE_GET_FUNCTION:
			printf("Executing cuModuleGetFuction...\n");
			cuda_result = get_module_function_of_client(&id_ptr, cmd->uint_args[0], cmd->str_args[0], client_handle);
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
		case LAUNCH_KERNEL:
			printf("Executing cuLaunchKernel...\n");
			cuda_result = launch_kernel_of_client(cmd->uint_args, cmd->n_uint_args, cmd->extra_args, cmd->n_extra_args);
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
		res = malloc_safe(sizeof(*res) * 2);
		res[1] = malloc_safe(sizeof(**res));
		res[1]->type = res_type;
		res[1]->elements = 1;
		res[1]->length = res_length;
		res[1]->data = malloc_safe(res_length);
		memcpy(res[1]->data, res_data, res_length);
		arg_count = 2;
	} else {
		res = malloc_safe(sizeof(*res));
		arg_count = 1;
	}
	res[0] = malloc_safe(sizeof(**res));
	res[0]->type = INT;
	res[0]->elements = 1;
	res[0]->length = sizeof(int);
	res[0]->data = malloc_safe(res[0]->length);
	memcpy(res[0]->data, &cuda_result, res[0]->length);

	*result = res;

	if (extra_args != NULL)
		free(extra_args);

	return arg_count;
}

int process_cuda_device_query(void **result, void *free_list, void *busy_list) {
	CudaDeviceList *cuda_devs;
	CudaDevice **cuda_devs_dev;
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
	cuda_devs = malloc_safe(sizeof(CudaDeviceList));
	cuda_device_list__init(cuda_devs);
	cuda_devs_dev = malloc_safe(sizeof(CudaDevice *) * cuda_dev_count);

	// Add devices
	printf("Adding devices...\n");
	// free
	i = 0;
	list_for_each_entry(pos, &free_list_p->node, node) {
		printf("%d -> %s\n", i, pos->cuda_device_name);
		cuda_devs_dev[i] = malloc_safe(sizeof(CudaDevice));
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
		cuda_devs_dev[i] = malloc_safe(sizeof(CudaDevice));
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

int pack_cuda_cmd(void **payload, var **args, size_t arg_count, int type) {
	CudaCmd *cmd;
	int i;

	printf("Packing CUDA cmd...\n");

	if (args == NULL) {
		return -1;
	}

	cmd = malloc_safe(sizeof(CudaCmd));
	cuda_cmd__init(cmd);

	cmd->type = type;
	cmd->arg_count = arg_count;

	for (i = 0; i < arg_count; i++) {	
		switch (args[i]->type) {
			case INT:
				cmd->n_int_args = args[i]->elements;
				cmd->int_args = args[i]->data;
				break;
			case UINT:
				cmd->n_uint_args = args[i]->elements;
				cmd->uint_args = args[i]->data;
				break;
			case STRING:
				cmd->n_str_args = args[i]->elements;
				cmd->str_args = args[i]->data;
				break;
			case BYTES:
				//cmd->n_extra_args = args[i]->elements;
				cmd->n_extra_args = 1;
				cmd->extra_args = malloc_safe(sizeof(*(cmd->extra_args)) * cmd->n_extra_args);
				cmd->extra_args[0].data = args[i]->data;
				cmd->extra_args[0].len = args[i]->length;
				break;
		}
	}

	*payload = cmd;
	return 0;
}

