#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
/*
typedef struct cuda_cmd_s {
	uint16_t type;
	uint32_t arg_count;
	//char *data;
} cuda_cmd;

typedef struct cookie_s {
	uint8_t type;
	uint32_t data_length;
	void *data;
} cookie;
*/
typedef enum var_type_e {
	UINT,
	BYTES
} var_type;

typedef struct var_s {
	var_type type;
	uint32_t length;
	void *data;
} var;

#define DEFAULT_PORT "8888"

enum {
	CUDA_CMD=0,
	CUDA_CMD_RESULT,
	CUDA_DEVICE_QUERY,
	CUDA_DEVICE_LIST,
	TEST,
	RESULT,
	INIT,
	DEVICE_GET,
	CONTEXT_CREATE,
	CONTEXT_DESTROY,
	MODULE_LOAD,
	MODULE_GET_FUNCTION,
	MEMORY_ALLOCATE,
	MEMORY_FREE,
	MEMCPY_HOST_TO_DEV,
	MEMCPY_DEV_TO_HOST,
	LAUNCH_KERNEL
};

#endif /* COMMON_H */
