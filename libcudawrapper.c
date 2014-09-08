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

static int client_sock_fd = -2;
static struct addrinfo server_addr;

CUresult cuInit(unsigned int Flags) {
	static CUresult (*cuInit_real) (unsigned int) = NULL;

	if (cuInit_real == NULL)
		cuInit_real = dlsym(RTLD_NEXT, "cuInit");
	
	get_server_connection(&client_sock_fd, &server_addr);

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
	
	get_server_connection(&client_sock_fd, &server_addr);
	
	arg.type = INT;
	arg.length = sizeof(int);
	arg.data = &ordinal;
	if (send_cuda_cmd(client_sock_fd, args, 1, DEVICE_GET) == -1) {
		fprintf(stderr, "Problem sending CUDA cmd!\n");
		exit(EXIT_FAILURE);
	}

	res_code = get_cuda_cmd_result(&result, client_sock_fd);

	device = result;
	
	return res_code; // cuDeviceGet_real(CUdevice *device, int ordinal);
}
