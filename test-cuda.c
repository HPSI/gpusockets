#include <stdio.h>
#include <cuda.h>

int main() {
	CUdevice device;

	cuInit(0);
	cuDeviceGet(&device, 0);
	printf("test-cuda Done...\n");
}
