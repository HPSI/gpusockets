#include <stdio.h>
#include <cuda.h>

int main() {
	CUdevice device;
	CUcontext context;
	CUmodule module;
	CUfunction function;

	cuInit(0);
	cuDeviceGet(&device, 0);
	cuCtxCreate(&context, 0, device);
	cuModuleLoad(&module, "matSumKernel.ptx");
	cuModuleGetFunction(&function, module, "matSum");
	cuCtxDestroy(context);
	//myCuFinish();
	printf("test-cuda Done...\n");

	return 0;
}
