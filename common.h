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

enum {
	CUDA_CMD,
	TEST
};
