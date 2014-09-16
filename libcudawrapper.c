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
	get_server_connection(&c_params);

	// Server should have already initialized CUDA Driver API,
	// so no need to send anything...

	return CUDA_SUCCESS; // cuInit_real(Flags);
}

CUresult cuDeviceGet(CUdevice *device, int ordinal) {
	static CUresult (*cuDeviceGet_real) (CUdevice *device, int ordinal) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };

	if (cuDeviceGet_real == NULL)
		cuDeviceGet_real = dlsym(RTLD_NEXT, "cuDeviceGet");

	// for testing
	close(c_params.sock_fd);
	// --
	get_server_connection(&c_params);
	
	arg.type = INT;
	arg.length = sizeof(int);
	arg.data = &ordinal;
	if (send_cuda_cmd(c_params.sock_fd, args, 1, DEVICE_GET) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);
	if (res_code == CUDA_SUCCESS)
		add_param_to_list(&c_params.device, *(uint64_t *) result);
	
	//device = result;
	
	return res_code; // cuDeviceGet_real(CUdevice *device, int ordinal);
}

CUresult cuCtxCreate(CUcontext* pctx, unsigned int flags, CUdevice dev) {
	static CUresult (*cuCtxCreate_real) (CUcontext* pctx, unsigned int flags, CUdevice dev) = NULL;
	void *result = NULL;
	CUresult res_code;
	var arg = { .elements = 1 }, *args[] = { &arg };

	if (cuCtxCreate_real == NULL)
		cuCtxCreate_real = dlsym(RTLD_NEXT, "cuCtxCreate");

	// for testing
	close(c_params.sock_fd);
	// --
	get_server_connection(&c_params);
	
	arg.type = INT;
	arg.length = sizeof(int);
	arg.data = &flags;
	if (send_cuda_cmd(c_params.sock_fd, args, 1, CONTEXT_CREATE) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, c_params.sock_fd);

	//pctx = result;
	
	return res_code; // cuCtxCreate_real(CUcontext* pctx, unsigned int flags, CUdevice dev);
}
