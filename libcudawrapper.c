#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cuda.h>
#include "common.h"
#include "common.pb-c.h"

CUresult cuInit(unsigned int Flags) {
	static CUresult (*cuInit_real) (unsigned int) = NULL;

	if (!cuInit_real)
		cuInit_real = dlsym(RTLD_NEXT, "cuInit");
	
	printf("libcudawrapper Here...");
	return; // cuInit_real(Flags);
}

CUresult cuDeviceGet(CUdevice *device, int ordinal) {
	static CUresult (*cuDeviceGet_real) (CUdevice *device, int ordinal) = NULL;

	if (!cuDeviceGet_real)
		cuDeviceGet_real = dlsym(RTLD_NEXT, "cuDeviceGet");
	
	printf("libcudawrapper Here...");
	return; // cuDeviceGet_real(CUdevice *device, int ordinal);
}
