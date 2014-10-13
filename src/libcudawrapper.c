#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <cuda.h>
#include <inttypes.h>

#include "common.h"
#include "common.pb-c.h"
#include "client.h"


//TODO: assert stored results' size fits...

static params c_params;
static unsigned int ctx_count = 0;

CUresult cuInit(unsigned int Flags) {
	static CUresult (*cuInit_real) (unsigned int) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	int param;

	if (cuInit_real == NULL)
		cuInit_real = dlsym(RTLD_NEXT, "cuInit");

	init_params(&c_params);	
	get_server_connection(&c_params);
	
	arg.type = INT;
	arg.length = sizeof(int);
	arg.data = &c_params.id;
	if (send_cuda_cmd(c_params.sock_fd, args, 1, INIT) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		c_params.id = *(uint64_t *) result;
		free(result);	
	}

	// Server should have already initialized CUDA Driver API,
	// so sending only the current client id (requesting a new one)...

	// for testing
	// close(c_params.sock_fd);
	// --

	return CUDA_SUCCESS; // cuInit_real(Flags);
}

CUresult cuDeviceGet(CUdevice *device, int ordinal) {
	static CUresult (*cuDeviceGet_real) (CUdevice *device, int ordinal) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	uint32_t param_id;

	if (cuDeviceGet_real == NULL)
		cuDeviceGet_real = dlsym(RTLD_NEXT, "cuDeviceGet");

	get_server_connection(&c_params);
	
	arg.type = INT;
	arg.length = sizeof(int);
	arg.data = &ordinal;
	if (send_cuda_cmd(c_params.sock_fd, args, 1, DEVICE_GET) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		param_id = add_param_to_list(&c_params.device, *(uint64_t *) result, NULL);
		memcpy(device, &param_id, sizeof(param_id));	
		free(result);	
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuDeviceGet_real
}

CUresult cuDeviceGetCount(int *count) {
	static CUresult (*cuDeviceGetCount_real) (int *count) = NULL;
	int tmp_count = 0;
	void *result = NULL;
	CUresult res_code;

	if (cuDeviceGetCount_real == NULL)
		cuDeviceGetCount_real = dlsym(RTLD_NEXT, "cuDeviceGetCount");

	get_server_connection(&c_params);
	
	if (send_cuda_cmd(c_params.sock_fd, NULL, 0, DEVICE_GET_COUNT) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		tmp_count = *(uint64_t *) result;
		memcpy(count, &tmp_count, sizeof(int));
		free(result);
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuDeviceGet_real(CUdevice *device, int ordinal);
}

CUresult cuDeviceGetName(char *name, int len, CUdevice dev) {
	static CUresult (*cuDeviceGetName_real) (char *name, int len, CUdevice dev) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg_int = { .elements = 1 }, arg_uint = { .elements = 1 },
		*args[] = { &arg_int, &arg_uint };
	uint32_t param_id;
	uint64_t param;

	if (cuDeviceGetName_real == NULL)
		cuDeviceGetName_real = dlsym(RTLD_NEXT, "cuDeviceGetName");

	get_server_connection(&c_params);

	arg_int.type = INT;
	arg_int.length = sizeof(int);
	arg_int.data = &len;

	arg_uint.type = UINT;
	arg_uint.length = sizeof(uint64_t);
	memcpy(&param_id, &dev, sizeof(uint32_t));
	param = get_param_from_list(c_params.device, param_id);
	arg_uint.data = &param;

	if (send_cuda_cmd(c_params.sock_fd, args, 2, DEVICE_GET_NAME) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		memcpy(name, result, len);
		free(result);
	}

	return res_code; // cuDeviceGetName_real
}

CUresult cuCtxCreate(CUcontext* pctx, unsigned int flags, CUdevice dev) {
	static CUresult (*cuCtxCreate_real) (CUcontext* pctx, unsigned int flags, CUdevice dev) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 2 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuCtxCreate_real == NULL)
		cuCtxCreate_real = dlsym(RTLD_NEXT, "cuCtxCreate");

	get_server_connection(&c_params);
	
	arg.type = UINT;
	arg.length = sizeof(uint64_t) * arg.elements;
	arg.data = malloc_safe(arg.length);
	memset(arg.data, 0, arg.length);
	memcpy(arg.data, &flags, sizeof(flags));
	memcpy(&param_id, &dev, sizeof(uint32_t));
	param = get_param_from_list(c_params.device, param_id);
	memcpy(arg.data+sizeof(uint64_t), &param, sizeof(param));
	if (send_cuda_cmd(c_params.sock_fd, args, 1, CONTEXT_CREATE) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		param_id = add_param_to_list(&c_params.context, *(uint64_t *) result, NULL);
		memcpy(pctx, &param_id, sizeof(param_id));
		++ctx_count;
		free(result);	
	} else if (res_code == -2) {
		fprintf(stderr," Requested CUDA device is busy!\n");
		res_code = CUDA_ERROR_INVALID_DEVICE;
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuCtxCreate_real(CUcontext* pctx, unsigned int flags, CUdevice dev);
}

CUresult cuCtxDestroy(CUcontext ctx) {
	static CUresult (*cuCtxDestroy_real) (CUcontext ctx) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuCtxDestroy_real == NULL)
		cuCtxDestroy_real = dlsym(RTLD_NEXT, "cuCtxDestroy");

	get_server_connection(&c_params);
	
	// TODO: 
	// - Free lists etc. 
	arg.type = UINT;
	if (ctx_count != 1) {
		arg.length = sizeof(uint64_t);
	} else {
		arg.length = 2 * sizeof(uint64_t);
		arg.elements = 2;
	}
	arg.data = malloc_safe(arg.length);
	memset(arg.data, 0, arg.length);
	memcpy(&param_id, &ctx, sizeof(uint32_t));
	param = get_param_from_list(c_params.context, param_id);
	memcpy(arg.data, &param, sizeof(param));
	if (ctx_count == 1)
		memcpy(arg.data+sizeof(uint64_t), &ctx_count, sizeof(ctx_count));

	if (send_cuda_cmd(c_params.sock_fd, args, 1, CONTEXT_DESTROY) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		remove_param_from_list(c_params.context, param_id);
		param_id = 0;
		memcpy(&ctx, &param_id, sizeof(param_id));
		--ctx_count;
		free(result);	
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuCtxDestroy_real(CUcontext ctx);
}

CUresult cuModuleLoad(CUmodule *module, const char *fname) {
	static CUresult (*cuModuleLoad_real) (CUmodule *module, const char *fname) = NULL;
	void *result = NULL, *file = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuModuleLoad_real == NULL)
		cuModuleLoad_real = dlsym(RTLD_NEXT, "cuModuleLoad");

	get_server_connection(&c_params);

	arg.type = BYTES;
	arg.length = read_cuda_module_file(&file, fname);
	arg.data = file;
	if (send_cuda_cmd(c_params.sock_fd, args, 1, MODULE_LOAD) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		param_id = add_param_to_list(&c_params.module, *(uint64_t *) result, NULL);
		memcpy(module, &param_id, sizeof(param_id));
		free(result);	
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuModuleLoad_real(CUmodule *module, const char *fname);
}

CUresult cuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name) {
	static CUresult (*cuModuleGetFunction_real)
		(CUfunction* hfunc, CUmodule hmod, const char* name) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg_uint = { .elements = 1 }, arg_str = { .elements = 1 },
		*args[] = { &arg_uint, &arg_str };
	uint32_t param_id;
	uint64_t param;

	if (cuModuleGetFunction_real == NULL)
		cuModuleGetFunction_real = dlsym(RTLD_NEXT, "cuModuleGetFunction");

	get_server_connection(&c_params);

	arg_uint.type = UINT;
	arg_uint.length = sizeof(uint64_t);
	memcpy(&param_id, &hmod, sizeof(uint32_t));
	param = get_param_from_list(c_params.module, param_id);
	arg_uint.data = &param;

	arg_str.type = STRING;
	arg_str.length = sizeof(char *);
	arg_str.data = &name;

	if (send_cuda_cmd(c_params.sock_fd, args, 2, MODULE_GET_FUNCTION) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		param_id = add_param_to_list(&c_params.function, *(uint64_t *) result, NULL);
		memcpy(hfunc, &param_id, sizeof(param_id));
		free(result);	
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuModuleGetFunction_real(CUfunction* hfunc, CUmodule hmod, const char* name);
}

CUresult cuMemAlloc(CUdeviceptr *dptr, size_t bytesize) {
	static CUresult (*cuMemAlloc_real) (CUdeviceptr *dptr, size_t bytesize) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuMemAlloc_real == NULL)
		cuMemAlloc_real = dlsym(RTLD_NEXT, "cuMemAlloc");

	get_server_connection(&c_params);

	arg.type = UINT;
	arg.length = sizeof(uint64_t);
	arg.data = &bytesize;

	if (send_cuda_cmd(c_params.sock_fd, args, 1, MEMORY_ALLOCATE) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		*dptr = *(uint64_t *) result;
		free(result);
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuMemAlloc_real(CUdeviceptr *dptr, size_t bytesize);
}

CUresult cuMemFree(CUdeviceptr dptr) {
	static CUresult (*cuMemFree_real) (CUdeviceptr dptr) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuMemFree_real == NULL)
		cuMemFree_real = dlsym(RTLD_NEXT, "cuMemFree");

	get_server_connection(&c_params);

	arg.type = UINT;
	arg.length = sizeof(uint64_t);
	arg.data = &dptr;

	if (send_cuda_cmd(c_params.sock_fd, args, 1, MEMORY_FREE) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		dptr = 0;
		free(result);
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuMemFree_real(CUdeviceptr dptr);
}

CUresult cuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount) {
	static CUresult (*cuMemcpyHtoD_real)
		(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg_uint = { .elements = 1 }, arg_b = { .elements = 1 },
		*args[] = { &arg_uint, &arg_b };
	uint32_t param_id;
	uint64_t param;

	if (cuMemcpyHtoD_real == NULL)
		cuMemcpyHtoD_real = dlsym(RTLD_NEXT, "cuMemcpyHtoD");

	get_server_connection(&c_params);

	arg_uint.type = UINT;
	arg_uint.length = sizeof(uint64_t);
	arg_uint.data = &dstDevice;
	
	arg_b.type = BYTES;
	arg_b.length = ByteCount;
	arg_b.data = srcHost;

	if (send_cuda_cmd(c_params.sock_fd, args, 2, MEMCPY_HOST_TO_DEV) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);

	if (result != NULL)
		free(result);	

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuMemcpyHtoD_real(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
}

CUresult cuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount) {
	static CUresult (*cuMemcpyDtoH_real)
		(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 2 }, *args[] = { &arg };
	uint32_t param_id;
	uint64_t param;

	if (cuMemcpyDtoH_real == NULL)
		cuMemcpyDtoH_real = dlsym(RTLD_NEXT, "cuMemcpyDtoH");

	get_server_connection(&c_params);
	
	arg.type = UINT;
	arg.length = sizeof(uint64_t) * arg.elements;
	arg.data = malloc_safe(arg.length);
	memset(arg.data, 0, arg.length);
	memcpy(arg.data, &srcDevice, sizeof(srcDevice));
	memcpy(arg.data+sizeof(uint64_t), &ByteCount, sizeof(ByteCount));
	if (send_cuda_cmd(c_params.sock_fd, args, 1, MEMCPY_DEV_TO_HOST) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		memcpy(dstHost, result, ByteCount);
		free(result);
	}

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuMemcpyDtoH_real(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
}

CUresult cuLaunchKernel(CUfunction f, unsigned int gridDimX,
		unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX,
	   	unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes,
	   	CUstream hStream, void **kernelParams, void **extra) {
	
	static CUresult (*cuLaunchKernel_real)
		(CUfunction f, unsigned int gridDimX, unsigned int gridDimY,
		 unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY,
		 unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream,
		 void **kernelParams, void **extra) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg_uint = { .elements = 9 }, arg_b = { .elements = 1 },
		*args[] = { &arg_uint, &arg_b };
	uint32_t param_id;
	uint64_t param;
	int i = 0;

	if (cuLaunchKernel_real == NULL)
		cuLaunchKernel_real = dlsym(RTLD_NEXT, "cuLaunchKernel");

	get_server_connection(&c_params);

	arg_uint.type = UINT;
	arg_uint.length = sizeof(uint64_t) * arg_uint.elements;
	arg_uint.data = malloc_safe(arg_uint.length);
	memset(arg_uint.data, 0, arg_uint.length);
	memcpy(arg_uint.data, &gridDimX, sizeof(gridDimX));
	memcpy(arg_uint.data+sizeof(uint64_t), &gridDimY, sizeof(gridDimY));
	memcpy(arg_uint.data+(sizeof(uint64_t)*2), &gridDimZ, sizeof(gridDimZ));
	memcpy(arg_uint.data+(sizeof(uint64_t)*3), &blockDimX, sizeof(blockDimX));
	memcpy(arg_uint.data+(sizeof(uint64_t)*4), &blockDimY, sizeof(blockDimY));
	memcpy(arg_uint.data+(sizeof(uint64_t)*5), &blockDimZ, sizeof(blockDimZ));
	memcpy(arg_uint.data+(sizeof(uint64_t)*6), &sharedMemBytes, sizeof(sharedMemBytes));
	memcpy(&param_id, &f, sizeof(uint32_t));
	param = get_param_from_list(c_params.function, param_id);
	memcpy(arg_uint.data+(sizeof(uint64_t)*7), &param, sizeof(param));
	if (hStream != 0) {
		memcpy(&param_id, &hStream, sizeof(uint32_t));
		param = get_param_from_list(c_params.stream, param_id);
	} else {
		param = 0;
	}
	memcpy(arg_uint.data+(sizeof(uint64_t)*8), &param, sizeof(param));

	if (kernelParams != NULL) {
		// TODO: implement params support...
	} else { 
		arg_b.type = BYTES;
		arg_b.data = NULL;
		arg_b.length = 0;
		do {
			if (extra[i] == CU_LAUNCH_PARAM_BUFFER_POINTER)
				arg_b.data = extra[++i];
			else if (extra[i] == CU_LAUNCH_PARAM_BUFFER_SIZE)
				arg_b.length = *(size_t *) extra[++i];

			++i;
		} while (extra[i] != NULL && extra[i] != CU_LAUNCH_PARAM_END);
	}
	
	if (send_cuda_cmd(c_params.sock_fd, args, 2, LAUNCH_KERNEL) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);

	if (result != NULL)
		free(result);	

	// for testing
	// close(c_params.sock_fd);
	// --

	return res_code; // cuLaunchKernel_real(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void **kernelParams, void **extra);
}

