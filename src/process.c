#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>

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

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t read_cuda_module_file(void **buffer, const char *filename) {
	size_t b_read = 0;
	FILE *fd;
	struct stat st;
	void *buf = NULL;

	gdprintf("Reading from file <%s> ... ", filename);
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
	gdprintf("read: %zu ... ", b_read);

	if (b_read != st.st_size) {
		fprintf(stderr, "Reading file failed: read %zu vs %jd expected\n", b_read, st.st_size);
		exit(EXIT_FAILURE);
	}

	*buffer = buf;

	fclose(fd);
	gdprintf("Done\n");

	return b_read;
}

void print_file_as_hex(uint8_t *file, size_t file_size) {
	int i;

	gdprintf("File size: %zu\n", file_size);
	for (i = 0; i < file_size; i++) {
		if (i % 14 == 0)
			gdprintf("\n");
		gdprintf("%02X ", (char) file[i]);
	}
	gdprintf("\n");
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

	gdprintf("Freeing list... ");
	list_for_each_entry_safe(pos, tmp, &cdn_list->node, node) {
		list_del(&pos->node);
		free(pos);
		i++;
	}
	gdprintf("%d nodes freed\n", i);
}

int add_device_to_list(cuda_device_node *dev_list, int dev_id) {
	cuda_device_node *cuda_dev_node;
	char cuda_dev_name[CUDA_DEV_NAME_MAX];
	CUdevice *cuda_device;
	CUcontext *cuda_context;

	cuda_device = malloc_safe(sizeof(CUdevice));
	cuda_dev_node = malloc_safe(sizeof(*cuda_dev_node));

	if (cuda_err_print(cuDeviceGet(cuda_device, dev_id), 0) != CUDA_SUCCESS)
		return -1;

	if (cuda_err_print(cuDeviceGetName(cuda_dev_name, CUDA_DEV_NAME_MAX, *cuda_device), 0) != CUDA_SUCCESS)
		return -1;

	cuda_context = malloc_safe(sizeof(*cuda_context));
	// Initializing per device context.
	// FIXME(?): We are ignoring client flags...
	if (cuda_err_print(cuCtxCreate(cuda_context, 0, *cuda_device), 0) != CUDA_SUCCESS)
		return -1;

	cuda_dev_node->cuda_device = cuda_device;
	strcpy(cuda_dev_node->cuda_device_name, cuda_dev_name);
	cuda_dev_node->cuda_context = cuda_context;
	cuda_dev_node->is_busy = 0;
	cuda_dev_node->client_count = 0;


	fprintf(stdout, "Adding device [%d]@%p -> %s, with context @%p\n", dev_id, cuda_dev_node->cuda_device, cuda_dev_node->cuda_device_name, cuda_dev_node->cuda_context);

	list_add_tail(&cuda_dev_node->node, &dev_list->node);

	return 0;
}

int discover_cuda_devices(void **free_list, void **busy_list) {
	int i, cuda_dev_count = 0;
	cuda_device_node *free_cuda_devs, *busy_cuda_devs;

	gdprintf("Discovering available devices...\n");
	cuda_err_print(cuInit(0), 1);

	// Get device count
	cuda_err_print(cuDeviceGetCount(&cuda_dev_count), 0);
	if (cuda_dev_count == 0) {
		fprintf(stderr, "No CUDA DEVICE available\n");
		exit(EXIT_FAILURE);
	}
	gdprintf("Available CUDA devices: %d\n", cuda_dev_count);

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

	gdprintf("\nAvailable CUDA devices:\n");
	free_list_p = free_list;
	busy_list_p = busy_list;

	gdprintf("| Free:\n");
	list_for_each_entry(pos, &free_list_p->node, node) {
		gdprintf("|   [%d] %s\n", i++, pos->cuda_device_name);
	}

	gdprintf("| Busy:\n");
	i = 0;
	list_for_each_entry(pos, &busy_list_p->node, node){
		gdprintf("|   [%d] %s\n", i++, pos->cuda_device_name);
	}
}

int add_client_to_list(client_node **client_handle, client_node *client_list) {
	client_node *new_node, *tmp;

	new_node = malloc_safe(sizeof(*new_node));

	if (!list_empty(&client_list->node)) {
		tmp = list_last_entry(&client_list->node, client_node, node);
		new_node->id = tmp->id + 1;
	} else {
		new_node->id = 0;
	}

	new_node->dev_count = 0;
	new_node->status = 1;
	new_node->cuda_dev_node = NULL;

	gdprintf("Adding client <%d> to list\n", new_node->id);
	list_add_tail(&new_node->node, &client_list->node);

	*client_handle = new_node;

	return 0;
}

int get_client_handle(client_node **client_handle, client_node **client_list, int client_id) {
	client_node *client_list_p = *client_list, *pos;

	pthread_mutex_lock(&client_mutex);
	if (client_list_p == NULL) {
		init_client_list(&client_list_p);
		*client_list = client_list_p;
	} else if (client_id >= 0) {
		// check if client exists in list
		list_for_each_entry(pos, &client_list_p->node, node) {
			if (pos->id == client_id) {
				gdprintf("Client <%d> is already in the list\n", client_id);
				*client_handle = pos;
				return 0;
			}
		}
	}

	add_client_to_list(client_handle, client_list_p);
	pthread_mutex_unlock(&client_mutex);

	return 0;
}

int del_client_of_list(client_node *client_handle) {
	client_node *client = client_handle;

	pthread_mutex_lock(&client_mutex);
	gdprintf("Deleting client <%d> from list\n", client->id);
	list_del(&client->node);
	pthread_mutex_unlock(&client_mutex);
	free(client_handle);

	return 0;
}

void print_clients(client_node *client_list) {
	client_node *client_list_p=client_list, *pos;
	int i = 0;

	pthread_mutex_lock(&client_mutex);
	gdprintf("\nClients:\n");
	list_for_each_entry(pos, &client_list_p->node, node) {
		gdprintf("| [%d] <%d>\n", i++, pos->id);
	}
	pthread_mutex_unlock(&client_mutex);
}

unsigned int get_client_status(client_node *client_handle) {
	client_node *client = client_handle;

	return (client_handle == NULL) ? 0 : client->status;
}

uint32_t add_param_to_list(param_node **list, uint64_t uintptr, void *relation) {
	uint32_t param_id = 0;
	param_node *new_node, *tmp;

	if (*list == NULL)
		init_param_list(list);
	else {
		// TODO(?): generate unique random id
		tmp = list_last_entry(&((*list)->node), param_node, node);
		param_id = tmp->id + 1;
	}

	new_node = malloc_safe(sizeof(*new_node));
	new_node->id = param_id;
	new_node->ptr = uintptr;
	new_node->rel = relation;

	list_add_tail(&new_node->node, &((*list)->node));

	return param_id;
}

int find_param_by_id(param_node **param, param_node *list, uint32_t param_id) {
	param_node *pos;

	list_for_each_entry(pos, &list->node, node) {
		if (pos->id == param_id) {
			*param = pos;
			return 0;
		}
	}

	return -1;
}

int find_param_by_ptr(param_node **param, param_node *list, uint64_t param_ptr) {
	param_node *pos;

	list_for_each_entry(pos, &list->node, node) {
		if (pos->ptr == param_ptr) {
			*param = pos;
			return 0;
		}
	}

	return -1;
}

int del_param_of_list(param_node *param) {
	list_del(&param->node);
	free(param);

	return 0;
}

int update_devices_of_client(uintptr_t *dev_node_ptr, cuda_device_node *free_list, int dev_ordinal, client_node *client) {
	cuda_device_node *tmp;
	int i = 0, true_ordinal;

	gdprintf("Updating devices of client <%d>...\n", client->id);

	tmp = list_first_entry_or_null(&free_list->node, cuda_device_node, node);
	if (tmp == NULL) {
		fprintf(stderr, "No CUDA devices available for assignment\n");
		return -1;
	}
	//true_ordinal = dev_ordinal - client->dev_count;
	while (i++ < dev_ordinal) {
		tmp = list_next_entry(tmp, node);
		if (&tmp->node == &free_list->node) {
			fprintf(stderr, "No CUDA devices available with the desired ordinal\n");
			return -1;
		}
	}

	*dev_node_ptr = (uintptr_t) tmp;
	// TODO: What if client deviceGets the same ordinal twice?
	add_param_to_list(&client->cuda_dev_node, (uintptr_t) tmp, NULL);

	return 0;
}

int attach_device_to_client(uintptr_t dev_node_ptr, cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *dev_node = (cuda_device_node *) dev_node_ptr;
	param_node *pos;

	pthread_mutex_lock(&client_mutex);
	gdprintf("Attaching device @%p to client <%d> ...\n", dev_node->cuda_device, client->id);

	list_for_each_entry(pos, &client->cuda_dev_node->node, node) {
		if ((cuda_device_node *) pos->ptr == dev_node) {
			dev_node->is_busy = 1;
			++client->dev_count;
			++dev_node->client_count;
			pthread_mutex_unlock(&client_mutex);

			return 0;
		}
	}
	pthread_mutex_unlock(&client_mutex);

	fprintf(stderr, "Requested CUDA device not in client's list!\n");
	return -1;
}

int detach_device_from_client(uintptr_t dev_node_ptr, cuda_device_node *free_list, cuda_device_node *busy_list, client_node *client) {
	cuda_device_node *dev_node = (cuda_device_node *) dev_node_ptr;
	param_node *pos, *tmp;

	pthread_mutex_lock(&client_mutex);
	gdprintf("Detaching device @%p from client <%d>...\n", dev_node->cuda_device, client->id);

	list_for_each_entry_safe(pos, tmp, &client->cuda_dev_node->node, node) {
		if ((cuda_device_node *) pos->ptr == dev_node) {
			del_param_of_list(pos);
			--client->dev_count;
			--dev_node->client_count;
			if (dev_node->client_count == 0)
				dev_node->is_busy = 0;
			pthread_mutex_unlock(&client_mutex);

			return 0;
		}
	}
	pthread_mutex_unlock(&client_mutex);

	fprintf(stderr, "Requested CUDA device not in client's list!\n");
	return -1;
}

int get_device_count_for_client(uint64_t *host_count) {
	CUresult res;
	int count;

	gdprintf("Getting CUDA device count...\n");

	res = cuda_err_print(cuDeviceGetCount(&count), 0);
	if (res == CUDA_SUCCESS)
		*host_count = count;

	return res;
}

int get_device_name_for_client(void **host_name_ptr, size_t *host_name_size, int name_size, uintptr_t dev_ptr) {
	CUresult res;
	cuda_device_node *dev_node = (cuda_device_node *) dev_ptr;

	gdprintf("Getting name of CUDA device @%p...\n", dev_node->cuda_device);

	*host_name_size = name_size;
	*host_name_ptr = malloc_safe(name_size);

	res = cuda_err_print(cuDeviceGetName(*host_name_ptr, name_size, *(dev_node->cuda_device)), 0);

	return res;
}


int get_context_of_client(uintptr_t *ctx_ptr, unsigned int flags, uintptr_t dev_node_ptr, client_node *client) {
	cuda_device_node *dev_node = (cuda_device_node *) dev_node_ptr;
	CUresult res;

	gdprintf("Getting CUDA context @%p of client <%d> ...\n", dev_node->cuda_context, client->id);
	res = cuda_err_print(cuCtxSetCurrent(*dev_node->cuda_context), 0);
	*ctx_ptr = dev_node_ptr;

	return res;
}

int put_context_of_client(uintptr_t *dev_node_ptr, uintptr_t ctx_ptr, client_node *client) {
	CUresult res;
	cuda_device_node *dev_node = (cuda_device_node *) ctx_ptr;

	// TODO: free modules/functions allocated handles (?)
	gdprintf("Putting CUDA context @%p of client <%d> ...\n", dev_node->cuda_context, client->id);
	res = cuda_err_print(cuCtxSetCurrent(NULL), 0);
	*dev_node_ptr = ctx_ptr;

	return res;
}

int load_module_of_client(uintptr_t *mod_ptr, ProtobufCBinaryData *image, client_node *client) {
	CUresult res;
	CUmodule *cuda_module;

	gdprintf("Loading CUDA module of client <%d> ... ", client->id);

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

	gdprintf("Loading CUDA module function of client <%d> ... ", client->id);

	cuda_func = malloc_safe(sizeof(*cuda_func));

	res = cuda_err_print(cuModuleGetFunction(cuda_func, *cuda_module, func_name), 0);

	if (res == CUDA_SUCCESS)
		*fun_ptr = (uintptr_t) cuda_func;

	return res;
}

int memory_allocate_for_client(uintptr_t *dev_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr cuda_dev_ptr;

	gdprintf("Allocating CUDA device memory of size %zuB...\n", mem_size);

	res = cuda_err_print(cuMemAlloc(&cuda_dev_ptr, mem_size), 0);
	if (res == CUDA_SUCCESS) {
		*dev_mem_ptr = cuda_dev_ptr;
		gdprintf("allocated @0x%llx\n", cuda_dev_ptr);
	}

	return res;
}

int memory_free_for_client(uintptr_t dev_mem_ptr) {
	CUresult res;
	CUdeviceptr cuda_dev_ptr = (CUdeviceptr) dev_mem_ptr;

	gdprintf("Freeing CUDA device memory @0x%llx...\n", cuda_dev_ptr);

	res = cuda_err_print(cuMemFree(cuda_dev_ptr), 0);

	return res;
}

int memcpy_host_to_dev_for_client(uintptr_t dev_mem_ptr, void *host_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr cuda_dev_ptr = (CUdeviceptr) dev_mem_ptr;

	gdprintf("Memcpying %zuB from host to CUDA device @0x%llx...\n", mem_size, cuda_dev_ptr);

	res = cuda_err_print(cuMemcpyHtoD(cuda_dev_ptr, host_mem_ptr, mem_size), 0);

	return res;
}

int memcpy_dev_to_host_for_client(void **host_mem_ptr, size_t *host_mem_size, uintptr_t dev_mem_ptr, size_t mem_size) {
	CUresult res;
	CUdeviceptr cuda_dev_ptr = (CUdeviceptr) dev_mem_ptr;

	gdprintf("Memcpying %zuB from CUDA device @0x%llx to host...\n", mem_size, cuda_dev_ptr);

	*host_mem_size = mem_size;
	*host_mem_ptr = malloc_safe(mem_size);

	res = cuda_err_print(cuMemcpyDtoH(*host_mem_ptr, cuda_dev_ptr, mem_size), 0);

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

	gdprintf("Executing kernel...\n");
	if (n_params > 0) {
		params = malloc_safe(sizeof(void *) * n_params);

		for(i = 0; i < n_params; i++) {
			params[i] = (void *) uints[9 + i];
		}
		gdprintf("using <params>\n");
	}
	if (n_extras > 0) {
		extra = malloc_safe(sizeof(void *) * 5);

		extra[0] = CU_LAUNCH_PARAM_BUFFER_POINTER;
		extra[1] = extras[0].data;
		extra[2] = CU_LAUNCH_PARAM_BUFFER_SIZE;
		extra[3] = &(extras[0].len);
		extra[4] = CU_LAUNCH_PARAM_END;
		gdprintf("using <extra>\n");
	}
	gdprintf("with grid (x, y, z) = (%u, %u, %u)\n", grid_x, grid_y, grid_z);
	gdprintf("and block (x, y, z) = (%u, %u, %u)\n", block_x, block_y, block_z);

	res = cuda_err_print(cuLaunchKernel(*func, grid_x, grid_y, grid_z,
				block_x, block_y, block_z, shared_mem_size, h_stream,
				params, extra), 0);

	if (params != NULL)
		free(params);

	if (extra != NULL)
		free(extra);

	return res;
}

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, client_node **client_list, client_node **client_handle) {
	int cuda_result = 0, arg_count = 0;
	CudaCmd *cmd = cmd_ptr;
	uint64_t uint_res = 0, tmp_ptr = 0;
	void *extra_args = NULL, *res_data = NULL;
	size_t extra_args_size = 0, res_length = 0;
	var **res = NULL;
	var_type res_type;

	if (*client_handle == NULL && cmd->type != INIT) {
		fprintf(stderr, "process_cuda_cmd: Invalid client handle\n");
		return -1;
	}

	gdprintf("Processing CUDA_CMD\n");
	switch(cmd->type) {
		case INIT:
			gdprintf("Executing cuInit...\n");
			get_client_handle(client_handle, client_list, cmd->int_args[0]);
			uint_res = (*client_handle)->id;
			// cuInit() should have already been executed by the server
			// by that point...
			//cuda_result = cuda_err_print(cuInit(cmd->uint_args[0]), 0);
			cuda_result = CUDA_SUCCESS;
			res_type = UINT;
			break;
		case DEVICE_GET:
			gdprintf("Executing cuDeviceGet...\n");
			if (update_devices_of_client(&uint_res, free_list, cmd->int_args[0], *client_handle) < 0)
				cuda_result = CUDA_ERROR_INVALID_DEVICE;
			else
				cuda_result = CUDA_SUCCESS;

			res_type = UINT;
			break;
		case DEVICE_GET_COUNT:
			gdprintf("Executing cuDeviceGetCount...\n");
			cuda_result = get_device_count_for_client(&uint_res);
			res_type = UINT;
			break;
		case DEVICE_GET_NAME:
			gdprintf("Executing cuDeviceGetName...\n");
			cuda_result = get_device_name_for_client(&extra_args, &extra_args_size, cmd->int_args[0], cmd->uint_args[0]);
			break;
		case CONTEXT_CREATE:
			gdprintf("Executing cuCtxCreate...\n");
			cuda_result = attach_device_to_client(cmd->uint_args[1], free_list, busy_list, *client_handle);
			if (cuda_result	< 0)
				break; // Handle appropriately in client.

			cuda_result = get_context_of_client(&uint_res, cmd->uint_args[0], cmd->uint_args[1], *client_handle);
			res_type = UINT;
			break;
		case CONTEXT_DESTROY:
			gdprintf("Executing cuCtxDestroy...\n");
			// We assume that only one context per device is created
			cuda_result = put_context_of_client(&tmp_ptr, cmd->uint_args[0], *client_handle);
			if (cuda_result == CUDA_SUCCESS) {
				detach_device_from_client(tmp_ptr, free_list, busy_list, *client_handle);
				if (cmd->n_uint_args > 1 && cmd->uint_args[1] == 1) {
					del_client_of_list(*client_handle);
					*client_handle = NULL;
				}
			}
			break;
		case MODULE_LOAD:
			gdprintf("Executing cuModuleLoad...\n");
			//print_file_as_hex(cmd->extra_args[0].data, cmd->extra_args[0].len);
			cuda_result = load_module_of_client(&uint_res, &(cmd->extra_args[0]), *client_handle);
			res_type = UINT;
			break;
		case MODULE_GET_FUNCTION:
			gdprintf("Executing cuModuleGetFuction...\n");
			cuda_result = get_module_function_of_client(&uint_res, cmd->uint_args[0], cmd->str_args[0], *client_handle);
			res_type = UINT;
			break;
		case MEMORY_ALLOCATE:
			gdprintf("Executing cuMemAlloc...\n");
			cuda_result = memory_allocate_for_client(&uint_res, cmd->uint_args[0]);
			res_type = UINT;
			break;
		case MEMORY_FREE:
			gdprintf("Executing cuMemFree...\n");
			cuda_result = memory_free_for_client(cmd->uint_args[0]);
			break;
		case MEMCPY_HOST_TO_DEV:
			gdprintf("Executing cuMemcpyHtoD...\n");
			cuda_result = memcpy_host_to_dev_for_client(cmd->uint_args[0], cmd->extra_args[0].data, cmd->extra_args[0].len);
			break;
		case MEMCPY_DEV_TO_HOST:
			gdprintf("Executing cuMemcpyDtoH...\n");
			cuda_result = memcpy_dev_to_host_for_client(&extra_args, &extra_args_size, cmd->uint_args[0], cmd->uint_args[1]);
			break;
		case LAUNCH_KERNEL:
			gdprintf("Executing cuLaunchKernel...\n");
			cuda_result = launch_kernel_of_client(cmd->uint_args, cmd->n_uint_args, cmd->extra_args, cmd->n_extra_args);
			break;
	}

	if (res_type == UINT) {
		res_length = sizeof(uint64_t);
		res_data = &uint_res;
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

	gdprintf("Processing CUDA_DEVICE_QUERY\n");
	list_for_each_entry(pos, &free_list_p->node, node) {
		cuda_dev_count++;
	}
	list_for_each_entry(pos, &busy_list_p->node, node){
		cuda_dev_count++;
	}
	gdprintf("Available CUDA devices: %d\n", cuda_dev_count);

	// Init variables
	cuda_devs = malloc_safe(sizeof(CudaDeviceList));
	cuda_device_list__init(cuda_devs);
	cuda_devs_dev = malloc_safe(sizeof(CudaDevice *) * cuda_dev_count);

	// Add devices
	gdprintf("Adding devices...\n");
	// free
	i = 0;
	list_for_each_entry(pos, &free_list_p->node, node) {
		gdprintf("%d -> %s\n", i, pos->cuda_device_name);
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
		gdprintf("%d -> %s\n", i, pos->cuda_device_name);
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

	gdprintf("Packing CUDA cmd...\n");

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

