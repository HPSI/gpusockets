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

int init_client(char *server_ip, char *port, struct sockaddr_in *addr) {
	int socket_fd, ret;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	bzero(addr, sizeof(*addr));
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = 0;    /* For wildcard IP address */
	hints.ai_protocol = IPPROTO_TCP;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(server_ip, port, &hints, &result);
	if (ret) {
		fprintf(stderr, "Error in getaddrinfo:%d : \n", ret, gai_strerror(ret));
		exit(ret);
	}
	socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (socket_fd < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

#if 0
	//addr->sin_family = AF_INET; // IPv4 addresses
	//addr->sin_port = htons(port);

	ret = inet_pton(AF_INET, server_ip, &addr->sin_addr);
	if (ret == 0) {
		perror("inet_pton failed: Invalid IP address string");
		exit(EXIT_FAILURE);
	} else if (ret < 0) {
		perror("inet_pton failed");
		exit(EXIT_FAILURE);
	}
#endif

	if (connect(socket_fd, (struct sockaddr*)result->ai_addr, result->ai_addrlen) < 0) {
		perror("connect failed");
		exit(EXIT_FAILURE);
	}

	return socket_fd;
}

int get_available_gpus(int sock_fd) {
	CudaDeviceList *devices;
	size_t buf_size, msg_length;
	void *buffer=NULL, *payload=NULL;

	printf("Sending request for available cuda devices...\n");
	buf_size = encode_message(&buffer, CUDA_DEVICE_QUERY, NULL);
	send_message(sock_fd, buffer, buf_size);
	if (buffer != NULL)
		free(buffer);
	
	printf("Waiting for response:\n");
	msg_length = receive_message(&buffer, sock_fd);
	if (msg_length > 0) {
		decode_message(&payload, buffer, msg_length);
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
		free(payload);
	}

	if (buffer != NULL)
		free(buffer);

	return 0;
}

int main(int argc, char *argv[]) {
	int client_sock_fd;
	size_t buf_size;
	char *server_port;
	struct sockaddr_in server_addr;
	char *server_ip, *a = "Hello", *b = "world";
	CudaCmd cmd = CUDA_CMD__INIT;
	void *buffer=NULL; 

	if (argc > 3 || argc < 2) {
		printf("Usage: client <server_ip> <server_port>\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 2) {
		printf("No port defined, using default %d\n", DEFAULT_PORT);
		server_port = (char *)DEFAULT_PORT;
	} else {
		server_port = argv[2];
	}
	server_ip = argv[1]; 

	client_sock_fd = init_client(server_ip, server_port, &server_addr);	
	printf("Connected to server %s on port %s...\n", server_ip, server_port);
	/*
	// build message payload
	cmd.type = TEST;
	cmd.arg_count = 4;
	cmd.n_int_args = 2;
	cmd.int_args = malloc(sizeof(int) * cmd.n_int_args);
	if (cmd.int_args == NULL) {
	fprintf(stderr, "cmd.int_args allocation failed\n");
	exit(EXIT_FAILURE);
	}
	cmd.int_args[0] = client_sock_fd;
	cmd.int_args[1] = 2;
	cmd.n_str_args = 2;
	cmd.str_args = malloc(sizeof(char *) * cmd.n_str_args);
	if (cmd.str_args == NULL) {
	fprintf(stderr, "cmd.str_args allocation failed\n");
	exit(EXIT_FAILURE);
	}
	cmd.str_args[0] = a;
	cmd.str_args[1] = b;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd);

	send_message(client_sock_fd, buffer, buf_size);

	free(cmd.int_args);
	free(cmd.str_args);
	free(buffer);
	*/
	get_available_gpus(client_sock_fd);

	printf("Message sent succesfully\n");
	close(client_sock_fd);

	return 0;
}
