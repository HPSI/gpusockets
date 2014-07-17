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
