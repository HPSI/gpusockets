#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include "common.h"
#include "common.pb-c.h"
#include "protocol.h"
#include "process.h"

int init_client(char *s_ip, char *s_port, struct addrinfo *s_addr) {
	int socket_fd, ret;
	struct addrinfo hints;

	bzero(&hints, sizeof(hints));
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

int get_available_gpus(int sock_fd) {
	CudaDeviceList *devices;
	size_t buf_size, msg_length;
	void *buffer=NULL, *payload=NULL, *dec_msg=NULL;

	printf("Sending request for available cuda devices...\n");
	buf_size = encode_message(&buffer, CUDA_DEVICE_QUERY, NULL);
	send_message(sock_fd, buffer, buf_size);
	if (buffer != NULL)
		free(buffer);
	
	printf("Waiting for response:\n");
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
		printf("Got response, free devices: %u\n", devices->devices_free);
		free_decoded_message(dec_msg);
	}

	if (buffer != NULL)
		free(buffer);

	return 0;
}

int64_t get_cuda_cmd_result(void **result, int sock_fd) {
	CudaCmd *cmd;
	size_t msg_length;
	void *buffer=NULL, *payload=NULL, *dec_msg=NULL;
	int res_code;

	printf("Waiting for response:\n");
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
		printf("Got response:\n| result code: %d\n", res_code);
		if (cmd->n_uint_args > 0) {
			*result = malloc(sizeof(uint64_t));
			if (*result == NULL) {
				fprintf(stderr, "result allocation failed\n");
				exit(EXIT_FAILURE);
			}
			memcpy(*result, &cmd->uint_args[0], sizeof(uint64_t));
			printf("| result: %p\n", (void *)*result);
		} else if (cmd->n_extra_args > 0) {
			*result = malloc(cmd->extra_args[0].len);
			if (*result == NULL) {
				fprintf(stderr, "result allocation failed\n");
				exit(EXIT_FAILURE);
			}
			memcpy(*result, cmd->extra_args[0].data, cmd->extra_args[0].len);
		}
		free_decoded_message(dec_msg);
	}

	if (buffer != NULL)
		free(buffer);

	return cmd->int_args[0];
}


int main(int argc, char *argv[]) {
	int client_sock_fd;
	size_t buf_size, file_size;
	char *server_port;
	struct addrinfo server_addr;
	char *server_ip, *a = "Hello", *b = "world";
	uint64_t ptr1;
	CudaCmd cmd1 = CUDA_CMD__INIT, cmd2 = CUDA_CMD__INIT,
			cmd3 = CUDA_CMD__INIT, cmd4 = CUDA_CMD__INIT,
			cmd5 = CUDA_CMD__INIT, cmd6 = CUDA_CMD__INIT,
			cmd7 = CUDA_CMD__INIT, cmd8 = CUDA_CMD__INIT,
			cmd9 = CUDA_CMD__INIT;
	void *buffer = NULL, *file = NULL, *result = NULL; 

	if (argc > 3 || argc < 2) {
		printf("Usage: client <server_ip> <server_port>\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 2) {
		printf("No port defined, using default %s\n", DEFAULT_PORT);
		server_port = (char *)DEFAULT_PORT;
	} else {
		server_port = argv[2];
	}
	server_ip = argv[1]; 

	client_sock_fd = init_client(server_ip, server_port, &server_addr);	
	printf("Connected to server %s on port %s...\n", server_ip, server_port);

	/**
	 * cmd1
	 **/	
	// build message payload
	cmd1.type = DEVICE_GET;
	cmd1.arg_count = 1;
	cmd1.n_int_args = 1;
	cmd1.int_args = malloc(sizeof(*(cmd1.int_args)) * cmd1.n_int_args);
	if (cmd1.int_args == NULL) {
		fprintf(stderr, "cmd1.int_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd1.int_args[0] = 0;
	/*
	cmd1.int_args[1] = 2;
	cmd1.n_str_args = 2;
	cmd1.str_args = malloc(sizeof(char *) * cmd1.n_str_args);
	if (cmd1.str_args == NULL) {
		fprintf(stderr, "cmd1.str_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd1.str_args[0] = a;
	cmd1.str_args[1] = b;
	*/
	buf_size = encode_message(&buffer, CUDA_CMD, &cmd1);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd1.int_args);
	free(buffer);	
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd2
	 **/	
	cmd2.type = CONTEXT_CREATE;
	cmd2.arg_count = 1;
	cmd2.n_uint_args = 1;
	cmd2.uint_args = malloc(sizeof(*(cmd2.uint_args)) * cmd2.n_uint_args);
	if (cmd2.uint_args == NULL) {
		fprintf(stderr, "cmd2.uint_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd2.uint_args[0] = 0;
	buf_size = encode_message(&buffer, CUDA_CMD, &cmd2);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd2.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd3
	 **/	
	cmd3.type = MODULE_LOAD;
	cmd3.arg_count = 1;
	cmd3.n_extra_args = 1;
	cmd3.extra_args = malloc(sizeof(*(cmd3.extra_args)) * cmd3.n_extra_args);
	if (cmd3.extra_args == NULL) {
		fprintf(stderr, "cmd3.extra_args allocation failed\n");
		exit(EXIT_FAILURE);
	}

	file_size = read_cuda_module_file(&file, "matSumKernel.ptx");
	
	cmd3.extra_args[0].data = file;
	cmd3.extra_args[0].len = file_size;
	//print_file_as_hex(cmd3.extra_args[0].data, cmd3.extra_args[0].len);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd3);
	send_message(client_sock_fd, buffer, buf_size);

	free(file);
	free(cmd3.extra_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd4
	 **/	
	cmd4.type = MODULE_GET_FUNCTION;
	cmd4.arg_count = 1;
	cmd4.n_str_args = 1;
	cmd4.str_args = malloc(sizeof(char *) * cmd4.n_str_args);
	if (cmd4.str_args == NULL) {
		fprintf(stderr, "cmd4.str_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd4.str_args[0] = "matSum";

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd4);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd4.str_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd5
	 **/	
	cmd5.type = MEMORY_ALLOCATE;
	cmd5.arg_count = 1;
	cmd5.n_uint_args =1;
	cmd5.uint_args = malloc(sizeof(*(cmd5.uint_args)) * cmd5.n_uint_args);
	if (cmd5.uint_args == NULL) {
		fprintf(stderr, "cmd5.uint_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd5.uint_args[0] = sizeof(int);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd5);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd5.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	ptr1 = *(uint64_t *)result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd6
	 **/	
	cmd6.type = MEMCPY_HOST_TO_DEV;
	cmd6.arg_count = 2;
	cmd6.n_extra_args = 1;
	cmd6.extra_args = malloc(sizeof(*(cmd6.extra_args)) * cmd6.n_extra_args);
	if (cmd6.extra_args == NULL) {
		fprintf(stderr, "cmd6.extra_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd6.extra_args[0].data = (void *)&client_sock_fd;
	cmd6.extra_args[0].len = sizeof(int);
	
	cmd6.n_uint_args =1;
	cmd6.uint_args = malloc(sizeof(*(cmd6.uint_args)) * cmd6.n_uint_args);
	if (cmd6.uint_args == NULL) {
		fprintf(stderr, "cmd6.uint_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd6.uint_args[0] = ptr1;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd6);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd6.extra_args);
	free(cmd6.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd7
	 **/	
	cmd7.type = MEMCPY_DEV_TO_HOST;
	cmd7.arg_count = 2;
	cmd7.n_uint_args = 2;
	cmd7.uint_args = malloc(sizeof(*(cmd7.uint_args)) * cmd7.n_uint_args);
	if (cmd7.uint_args == NULL) {
		fprintf(stderr, "cmd7.uint_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd7.uint_args[0] = ptr1;
	cmd7.uint_args[1] = sizeof(int);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd7);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd7.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd8
	 **/	
	cmd8.type = MEMORY_FREE;
	cmd8.arg_count = 1;
	cmd8.n_uint_args =1;
	cmd8.uint_args = malloc(sizeof(*(cmd8.uint_args)) * cmd8.n_uint_args);
	if (cmd8.uint_args == NULL) {
		fprintf(stderr, "cmd8.uint_args allocation failed\n");
		exit(EXIT_FAILURE);
	}
	cmd8.uint_args[0] = ptr1;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd8);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd8.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd9
	 **/	
	cmd9.type = CONTEXT_DESTROY;
	cmd9.arg_count = 0;
	buf_size = encode_message(&buffer, CUDA_CMD, &cmd9);
	send_message(client_sock_fd, buffer, buf_size);

	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	//get_available_gpus(client_sock_fd);

	printf("Message sent succesfully\n");
	close(client_sock_fd);

	return 0;
}
