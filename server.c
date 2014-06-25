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
#define MAX_MSG_SIZE 2048

int init_server(in_port_t port, struct sockaddr_in *addr) {
	int socket_fd;

	bzero(addr, sizeof(*addr));

	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	addr->sin_family = AF_INET; // IPv4 addresses
	addr->sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
	addr->sin_port = htons(port);

	if (bind(socket_fd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(socket_fd, 10) < 0) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	return socket_fd;
}

ssize_t read_socket(int fd, void *buffer, size_t bytes) {
	ssize_t	b_read, b_total = 0;

	do {
		b_read = read(fd, buffer+b_total, bytes-b_total);
		if (b_read < 0) {
			perror("read socket failed");
			exit(EXIT_FAILURE);
		}
		b_total += b_read;
	} while (b_total < bytes);
	
	printf("Bytes received: %zd\n", b_total);
	return b_total;
}

void decode_message(Cookie *msg) {
	int i;
	Cmd *cmd;

	printf("Decoding message data...\n");
	switch (msg->type) {
		case CUDA_CMD:
			printf("--------------\nIs cuda cmd\n");
							
			cmd = msg->payload;
			printf("Decoding cmd data...\n");

			switch (cmd->type) {
				case TEST:
					printf("-Type: TEST\n");
					printf("-Arguments: %u\n", cmd->arg_count);
					for (i = 0; i < cmd->n_int_args; i++) { 
						printf ("--int: %d\n", cmd->int_args[i]);
					}
					for (i = 0; i < cmd->n_str_args; i++) { 
						printf ("--str: %s\n", cmd->str_args[i]);
					}

					break;
			}
			
			break;
	}
}

int main(int argc, char *argv[]) {
	int server_sock_fd, client_sock_fd;
	in_port_t local_port;
	struct sockaddr_in local_addr, client_addr;
	socklen_t s;
    char client_ip[INET_ADDRSTRLEN], *a, *b;
	Cookie *message;
	void *buffer;
	uint32_t msg_length;

	if (argc != 2) {
		printf("Usage: server <local_port>\n");
		exit(EXIT_FAILURE);
	}
	local_port = atoi(argv[1]);
	
	server_sock_fd = init_server(local_port, &local_addr);	
	printf("Server listening on port %d for incoming connections...\n", local_port);

	for (;;) {
		s = sizeof(client_addr);
		client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&client_addr, &s);
		if (client_sock_fd < 0) {
			perror("accept failed");
			exit(EXIT_FAILURE);
		}

		printf("\nConnection accepted ");
		if (inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)) == NULL)
			printf("from client with unknown ip");
		else
			printf("from client %s\n", client_ip);

		buffer = malloc(sizeof(uint32_t));
		if (buffer == NULL) {
			perror("buffer memory allocation failed");
			exit(EXIT_FAILURE);
		}
		// read message length
		read_socket(client_sock_fd, buffer, sizeof(uint32_t));

		msg_length = ntohl(*(uint32_t *)buffer);
		printf("Going to read a message of %u bytes...\n", msg_length);
		
		buffer = realloc(buffer, msg_length);
		if (buffer == NULL) {	
			perror("buffer memory reallocation failed");
			exit(1);
		}
		// read message
		read_socket(client_sock_fd, buffer, msg_length);
		
		message = cookie__unpack(NULL, msg_length, (uint8_t *)buffer);
		if (message == NULL) {
			fprintf(stderr, "message unpacking failed\n");
			continue;
		}
		free(buffer);
		
		decode_message(message);
		printf("--------------\nMessage processed, cleaning up...\n");
		cookie__free_unpacked(message, NULL);
		close(client_sock_fd);
	}
}
