#include <stdio.h>
#include <string.h>
#include <cuda.h>

#include "common.h"

int main() {
	CUdevice device;
	CUcontext context;
	CUmodule module;
	CUfunction function;
	CUdeviceptr d_a, d_b, d_c;
	int a = 10, b = 12, c;
	size_t arg_len = 3 * sizeof(CUdeviceptr);
	void *params[] = { &d_a, &d_b, &d_c },
		 *args = NULL,
		 *extra[] = {
			 CU_LAUNCH_PARAM_BUFFER_POINTER,
			 NULL,
			 CU_LAUNCH_PARAM_BUFFER_SIZE,
			 &arg_len,
			 CU_LAUNCH_PARAM_END
		 };


	cuInit(0);

	printf("\n* Get device and create context... *\n");
	cuDeviceGet(&device, 0);
	cuCtxCreate(&context, 0, device);

	printf("\n* Load module and get function... *\n");
	cuModuleLoad(&module, "matSumKernel.ptx");
	cuModuleGetFunction(&function, module, "matSum");

	printf("\n* Allocate device memory... *\n");
	cuMemAlloc(&d_a, sizeof(int));
	cuMemAlloc(&d_b, sizeof(int));
	cuMemAlloc(&d_c, sizeof(int));

	printf("\n* Copy host variable values to device... *\n");
	cuMemcpyHtoD(d_a, &a, sizeof(a));
	cuMemcpyHtoD(d_b, &b, sizeof(b));

	printf("\n* Execute device kernel... *\n");
	args = malloc_safe(arg_len);
	memcpy(args, &d_a, sizeof(d_a));
	memcpy(args+sizeof(d_a), &d_b, sizeof(d_b));
	memcpy(args+(2*sizeof(d_a)), &d_c, sizeof(d_c));
	extra[1] = args; 
	
	cuLaunchKernel(function, 1, 1, 1, 1, 1, 1, 0, 0, 0, extra);
	
	printf("\n* Copy device result to host... *\n");
	cuMemcpyDtoH(&c, d_c, sizeof(c));
	printf("|\n--> Execution Result: %d\n\n", c);

	printf("\n* Free device memory... *\n");
	cuMemFree(d_a);
	cuMemFree(d_b);
	cuMemFree(d_c);

	printf("\n* Destroy device context... *\n");
	cuCtxDestroy(context);
	//myCuFinish();
	printf("test-cuda Done...\n");

	return 0;
}
