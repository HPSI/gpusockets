#ifndef PROCESS_H
#define PROCESS_H

#include <cuda.h>
#include "list.h"
#include "common.h"
#include "timer.h"

#define CUDA_DEV_NAME_MAX 100
typedef struct param_node_s {
	struct list_head node;
	uint32_t id;
	uint64_t ptr;
	void *rel;
} param_node;

typedef struct cuda_device_node_s {
	CUdevice *cuda_device;
	struct list_head node;
	char cuda_device_name[CUDA_DEV_NAME_MAX];
	CUcontext *cuda_context;
	unsigned int client_count;
	int is_busy;
} cuda_device_node;

typedef struct client_node_s {
	int id;
	unsigned int dev_count;
	unsigned int status;
	struct list_head node;
	param_node *cuda_dev_node;
} client_node;


size_t read_cuda_module_file(void **buffer, const char *filename);

int discover_cuda_devices(void **free_list, void **busy_list);

void print_cuda_devices(void *free_list, void *busy_list);

void print_clients(client_node *client_list);

unsigned int get_client_status(client_node *client_handle);

uint32_t add_param_to_list(param_node **list, uint64_t uintptr, void *relation);

int find_param_by_id(param_node **param, param_node *list, uint32_t param_id);

int del_param_of_list(param_node *param);

int process_cuda_cmd(void **result, void *cmd_ptr, void *free_list, void *busy_list, client_node **client_list, client_node **client_handle);

int process_cuda_device_query(void **result, void *free_list, void *busy_list);

void free_cdn_list(void *list);

int pack_cuda_cmd(void **payload, var **args, size_t arg_count, int type);


extern __thread gs_timer ps_i, ps_dg, ps_dgc, ps_dgca, ps_dgn, ps_dgna,
						 ps_cc, ps_cca, ps_cd, ps_cda, ps_ml, ps_mla,
						 ps_mgf, ps_mgfa, ps_ma, ps_maa, ps_mf, ps_mfa,
						 ps_mhd, ps_mhda, ps_mdh, ps_mdha, ps_lk, ps_lka,
						 ps_ext;

#endif /* PROCESS_H */
