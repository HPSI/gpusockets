#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <inttypes.h>

#include "client.h"
#include "common.h"
#include "common.pb-c.h"
#include "protocol.h"
#include "process.h"
#include "timer.h"

int init_client(const char *s_ip, const char *s_port, struct addrinfo *s_addr) {
	int socket_fd, ret;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;	// Allow IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;	// For wildcard IP address
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(s_ip, s_port, &hints, &s_addr);
	if (ret) {
		fprintf(stderr, "getaddrinfo failed: [%d] %s\n", ret, gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	socket_fd = socket(s_addr->ai_family, s_addr->ai_socktype, s_addr->ai_protocol);
	if (socket_fd < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	if (connect(socket_fd, (struct sockaddr*)s_addr->ai_addr, s_addr->ai_addrlen) < 0) {
		perror("connect failed");
		exit(EXIT_FAILURE);
	}

	return socket_fd;
}

void init_params(params *p) {
	p->id = -1;
	p->device = NULL;
	p->context = NULL;
	p->module = NULL;
	p->function = NULL;
	p->variable = NULL;
	p->stream = NULL;
}

uint64_t get_param_from_list(param_node *list, uint32_t param_id) {
	param_node *param = NULL;

	if (find_param_by_id(&param, list, param_id) != 0) {
		fprintf(stderr, "Requested param not in given list!\n");
		return 0;
	}

	return param->ptr;
}

int remove_param_from_list(param_node *list, uint32_t param_id) {
	param_node *param = NULL;

	if (find_param_by_id(&param, list, param_id) != 0) {
		fprintf(stderr, "Requested param not in given list!\n");
		return -1;
	}

	del_param_of_list(param);
	return 0;
}

void get_server_connection(params *p) {
	char s_ip[16], s_port[6];

	if (p->id < 0) {
		if (get_server_ip(s_ip, s_port) != 0) {
			sprintf(s_ip, DEFAULT_SERVER_IP);
			sprintf(s_port, DEFAULT_SERVER_PORT);
			gdprintf("Could not get env vars, using defaults: %s:%s\n", s_ip, s_port);
		}
		p->sock_fd = init_client(s_ip, s_port, &(p->addr));
		gdprintf("Connected to server %s on port %s...\n", s_ip, s_port);
	}
}

int64_t get_cuda_cmd_result(void **result, int sock_fd) {
	CudaCmd *cmd;
	size_t msg_length;
	void *buffer=NULL, *payload=NULL, *dec_msg=NULL;
	int res_code;

	gs_timer tm;
	TIMER_RESET(&tm);
	TIMER_START(&tm);
	gdprintf("Waiting for response:\n");
	msg_length = receive_message(&buffer, sock_fd);
	if (msg_length > 0) {
		decode_message(&dec_msg, &payload, buffer, msg_length);
	} else {
		fprintf(stderr, "Problem receiving response!\n");
		exit(EXIT_FAILURE);
	}

	if (payload == NULL) {
		fprintf(stderr, "Problem decoding response!\n");
		exit(EXIT_FAILURE);
	} else {
		cmd = payload;
		res_code = cmd->int_args[0];
		gdprintf("Got response:\n| result code: %d\n", res_code);
		if (cmd->n_uint_args > 0) {
			*result = malloc_safe(sizeof(uint64_t));
			memcpy(*result, &cmd->uint_args[0], sizeof(uint64_t));
			gdprintf("| result: 0x%" PRIx64 "\n", *(uint64_t *) *result);
		} else if (cmd->n_extra_args > 0) {
			*result = malloc_safe(cmd->extra_args[0].len);
			memcpy(*result, cmd->extra_args[0].data, cmd->extra_args[0].len);
			gdprintf("| result: (bytes)\n");
		}
		free_decoded_message(dec_msg);
	}

	if (buffer != NULL)
		free(buffer);

	TIMER_STOP(&tm);
	gdprintf("\nClient Receive: %lf\n", TIMER_TO_SEC(TIMER_TOTAL(&tm)));

	return res_code;
}

int get_available_gpus(int sock_fd) {
	CudaDeviceList *devices;
	size_t buf_size, msg_length;
	void *buffer=NULL, *payload=NULL, *dec_msg=NULL;

	gdprintf("Sending request for available cuda devices...\n");
	buf_size = encode_message(&buffer, CUDA_DEVICE_QUERY, NULL);
	send_message(sock_fd, buffer, buf_size);
	if (buffer != NULL)
		free(buffer);

	gdprintf("Waiting for response:\n");
	msg_length = receive_message(&buffer, sock_fd);
	if (msg_length > 0) {
		decode_message(&dec_msg, &payload, buffer, msg_length);
	} else {
		fprintf(stderr, "Problem receiving response!\n");
		exit(EXIT_FAILURE);
	}

	if (payload == NULL) {
		fprintf(stderr, "Problem decoding response!\n");
		exit(EXIT_FAILURE);
	} else {
		devices = payload;
		gdprintf("Got response, free devices: %u\n", devices->devices_free);
		free_decoded_message(dec_msg);
	}

	if (buffer != NULL)
		free(buffer);

	return 0;
}

int send_cuda_cmd(int sock_fd, var **args, size_t arg_count, int type) {
	void *buffer = NULL, *payload = NULL;
	size_t buf_size;

	gs_timer tm;
	TIMER_RESET(&tm);
	TIMER_START(&tm);

	gdprintf("Sendind CUDA cmd...\n");
	pack_cuda_cmd(&payload, args, arg_count, type);

	buf_size = encode_message(&buffer, CUDA_CMD, payload);
	if (buffer == NULL)
		return -1;

	send_message(sock_fd, buffer, buf_size);

	free(buffer);

	TIMER_STOP(&tm);
	gdprintf("\nClient Send: %lf\n", TIMER_TO_SEC(TIMER_TOTAL(&tm)));

	return 0;
}


