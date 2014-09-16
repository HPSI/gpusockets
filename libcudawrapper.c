#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <cuda.h>

#include "common.h"
#include "common.pb-c.h"
#include "client.h"

static params c_params;

CUresult cuInit(unsigned int Flags) {
	static CUresult (*cuInit_real) (unsigned int) = NULL;

	if (cuInit_real == NULL)
		cuInit_real = dlsym(RTLD_NEXT, "cuInit");
	
	init_params(&c_params);	
	//get_server_connection(&c_params);

	// Server should have already initialized CUDA Driver API,
	// so no need to send anything...

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
		param_id = add_param_to_list(&c_params.device, *(uint64_t *) result);
		memcpy(device, &param_id, sizeof(param_id));
	}
	
	// for testing
	close(c_params.sock_fd);
	// --

	return res_code; // cuDeviceGet_real(CUdevice *device, int ordinal);
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
	bzero(arg.data, arg.length);
	memcpy(arg.data, &flags, sizeof(uint64_t));
	memcpy(&param_id, &dev, sizeof(uint32_t));
	param = get_param_from_list(c_params.device, param_id);
	memcpy(arg.data+sizeof(uint64_t), &param, sizeof(uint32_t));
	if (send_cuda_cmd(c_params.sock_fd, args, 1, CONTEXT_CREATE) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS)
		param_id = add_param_to_list(&c_params.context, *(uint64_t *) result);
	
	memcpy(pctx, &param_id, sizeof(param_id));

	// for testing
	close(c_params.sock_fd);
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
	// - Send finshed message if CtxDestroy is the last.
	arg.type = UINT;
	arg.length = sizeof(uint64_t) * arg.elements;
	arg.data = malloc_safe(arg.length);
	bzero(arg.data, arg.length);
	memcpy(&param_id, &ctx, sizeof(uint32_t));
	param = get_param_from_list(c_params.context, param_id);
	memcpy(arg.data, &param, sizeof(uint32_t));
	if (send_cuda_cmd(c_params.sock_fd, args, 1, CONTEXT_DESTROY) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS)
		remove_param_from_list(c_params.context, param_id);

	param_id = 0;	
	memcpy(&ctx, &param_id, sizeof(param_id));

	// for testing
	close(c_params.sock_fd);
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
		param_id = add_param_to_list(&c_params.module, *(uint64_t *) result);
		memcpy(module, &param_id, sizeof(param_id));
	}

	// for testing
	close(c_params.sock_fd);
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
	arg_uint.length = sizeof(uint64_t) * arg_uint.elements;
	arg_uint.data = malloc_safe(arg_uint.length);
	bzero(arg_uint.data, arg_uint.length);
	memcpy(&param_id, &hmod, sizeof(uint32_t));
	param = get_param_from_list(c_params.module, param_id);
	memcpy(arg_uint.data, &param, sizeof(uint32_t));

	arg_str.type = STRING;
	arg_str.length = sizeof(char *);
	arg_str.data = &name;

	if (send_cuda_cmd(c_params.sock_fd, args, 2, MODULE_GET_FUNCTION) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS) {
		param_id = add_param_to_list(&c_params.function, *(uint64_t *) result);
		memcpy(hfunc, &param_id, sizeof(param_id));
	}

	// for testing
	close(c_params.sock_fd);
	// --

	return res_code; // cuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name);
}
