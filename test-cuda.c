#include <stdio.h>
#include <cuda.h>

int main() {
	CUdevice device;
	CUcontext context;
	CUmodule module;
	CUfunction function;
	CUdeviceptr d_a, d_b, d_c;
	int a = 10, b = 12, c;
	void *args[3] = { &d_a, &d_b, &d_c };

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
	/*
	printf("\n* Execute device kernel... *\n");
	cuLaunchKernel(function, 1, 1, 1, 1, 1, 1, 0, 0, args, 0);
	
	printf("\n* Copy device result to host... *\n");
	cuMemcpyDtoH(&c, d_c, sizeof(c));
	*/
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
