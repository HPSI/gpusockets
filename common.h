#ifndef COMMON_H
#define COMMON_H

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

#define DEFAULT_PORT "8888"

enum {
	CUDA_CMD,
	CUDA_CMD_RESULT,
	CUDA_DEVICE_QUERY,
	CUDA_DEVICE_LIST,
	TEST,
	INIT,
	DEVICE_GET,
	CONTEXT_CREATE,
	CONTEXT_DESTROY
};

#endif /* COMMON_H */
