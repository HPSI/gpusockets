#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.h"
#include "common.pb-c.h"

int init_client(char *server_ip, in_port_t port, struct sockaddr_in *addr) {
	int socket_fd, ret;

	bzero(addr, sizeof(*addr));

	socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_fd < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	addr->sin_family = AF_INET; // IPv4 addresses
	addr->sin_port = htons(port);

	ret = inet_pton(AF_INET, server_ip, &addr->sin_addr);
	if (ret == 0) {
		perror("inet_pton failed: Invalid IP address string");
		exit(EXIT_FAILURE);
	} else if (ret < 0) {
		perror("inet_pton failed");
		exit(EXIT_FAILURE);
	}

	if (connect(socket_fd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
		perror("connect failed");
		exit(EXIT_FAILURE);
	}

	return socket_fd;
}

ssize_t write_socket(int fd, void *buffer, size_t bytes) {
	ssize_t b_written, b_total;

	b_total = 0;
	do {
		b_written = write(fd, buffer+b_total, bytes-b_total);
		if (b_written < 0) {
			perror("write socket failed");
			exit(EXIT_FAILURE);
		}
		b_total += b_written;
	} while (b_total < bytes);
	
	printf("Bytes sent: %zd\n", b_total);
	return b_total;
}

int main(int argc, char *argv[]) {
	int client_sock_fd;
	size_t buf_size;
	uint32_t msg_length, msg_len_n;
	in_port_t server_port;
	struct sockaddr_in server_addr;
	char *server_ip, *a = "Hello", *b = "world";
	Cookie message = COOKIE__INIT;
	Cmd cmd = CMD__INIT;
	void *buffer, *msg_buffer; 

	if (argc != 3) {
		printf("Usage: client <server_ip> <server_port>\n");
		exit(EXIT_FAILURE);
	}
	server_ip = argv[1];
	server_port = atoi(argv[2]);

	client_sock_fd = init_client(server_ip, server_port, &server_addr);	
	printf("Connected to server %s on port %d...\n", server_ip, server_port);

	message.type = CUDA_CMD;
	cmd.type = TEST;
	cmd.arg_count = 4;
	cmd.n_int_args = 2;
	cmd.int_args = malloc(sizeof(int) * cmd.n_int_args);
	if (cmd.int_args == NULL) {
		perror("cmd.int_args allocation failed");
		exit(EXIT_FAILURE);
	}
	cmd.int_args[0] = client_sock_fd;
	cmd.int_args[1] = 2;
	cmd.n_str_args = 2;
	cmd.str_args = malloc(sizeof(char *) * cmd.n_str_args);
	if (cmd.str_args == NULL) {
		perror("cmd.str_args allocation failed");
		exit(EXIT_FAILURE);
	}
	cmd.str_args[0] = a;
	cmd.str_args[1] = b;
	
	message.payload = &cmd;	
	msg_length = cookie__get_packed_size(&message);
	msg_buffer = malloc(msg_length);
	if (msg_buffer == NULL) {
		perror("msg_buffer memory allocation failed");
		exit(EXIT_FAILURE);
	}
	cookie__pack(&message, msg_buffer);
	msg_len_n = htonl(msg_length);
	
	buf_size = msg_length + sizeof(msg_len_n);
	buffer = malloc(buf_size);
	if (buffer == NULL) {
		perror("buffer memory allocation failed");
		exit(EXIT_FAILURE);
	}
	memcpy(buffer, &msg_len_n, sizeof(msg_len_n));
	memcpy(buffer+sizeof(msg_len_n), msg_buffer, msg_length);

	printf("Going to send %zd bytes...\n", buf_size);
	write_socket(client_sock_fd, buffer, buf_size);

	printf("Message sent succesfully\n");
	free(cmd.int_args);
	free(cmd.str_args);
	free(buffer);
	free(msg_buffer);
	close(client_sock_fd);
}
