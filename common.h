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
	INT,
	UINT,
	STRING,
	BYTES
} var_type;

typedef struct var_s {
	var_type type;
	uint32_t length;
	uint32_t elements;
	void *data;
} var;

#define SERVER_IP "localhost"
#define SERVER_PORT "8888"

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

inline void *malloc_safe_f(size_t size, const char *file, const int line); 
#define malloc_safe(size) malloc_safe_f(size, __FILE__, __LINE__)

inline void *realloc_safe_f(void *ptr, size_t size, const char *file, const int line); 
#define realloc_safe(ptr, size) realloc_safe_f(ptr, size, __FILE__, __LINE__)

inline void *calloc_safe_f(size_t nmemb, size_t size, const char *file, const int line); 
#define calloc_safe(nmemb, size) calloc_safe_f(nmemb, size, __FILE__, __LINE__)

#ifdef GPUSOCK_DEBUG
#define gdprintf printf
#else
#define gdprintf
#endif

#endif /* COMMON_H */
