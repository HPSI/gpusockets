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
#include "protocol.h"
#include "process.h"

int init_server_net(in_port_t port, struct sockaddr_in *addr) {
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

int init_server(in_port_t port, struct sockaddr_in *addr, void **free_list, void **busy_list) {
	int socket_fd;

	printf("Initializing server...\n");
	socket_fd = init_server_net(port, addr);
	discover_cuda_devices(free_list, busy_list);

	return socket_fd;
}

int main(int argc, char *argv[]) {
	int server_sock_fd, client_sock_fd, msg_type, cuda_dev_arr_size, resp_type;
	in_port_t local_port;
	struct sockaddr_in local_addr, client_addr;
	socklen_t s;
    char client_ip[INET_ADDRSTRLEN];
	void *msg=NULL, *payload, *result=NULL, *cuda_dev_array=NULL, *free_list=NULL, *busy_list=NULL;
	uint32_t msg_length;

	if (argc > 2) {
		printf("Usage: server <local_port>\n");
		exit(EXIT_FAILURE);
	}
	
	if (argc == 1) {
		printf("No port defined, using default %d\n", DEFAULT_PORT);
		local_port = atoi(DEFAULT_PORT);
	} else {
		local_port = atoi(argv[1]);
	}
	
	server_sock_fd = init_server(local_port, &local_addr, &free_list, &busy_list);
	print_cuda_devices(free_list, busy_list);
	printf("\nServer listening on port %d for incoming connections...\n", local_port);

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

		msg_length = receive_message(&msg, client_sock_fd);
		if (msg_length > 0)
			msg_type = decode_message(&payload, msg, msg_length);
			
		if (msg != NULL) {
			free(msg);
			msg = NULL;
		}
		
		//CudaDeviceList *sth;, *cuda_dev_array=NULL;
		printf("Processing message\n");
		switch (msg_type) {
			case CUDA_CMD:
				process_cuda_cmd(&result, payload);
				resp_type = CUDA_CMD_RESULT;
				break;
			case CUDA_DEVICE_QUERY:
				process_cuda_device_query(&result, &cuda_dev_array, &cuda_dev_arr_size);
				resp_type = CUDA_DEVICE_LIST;
				// -- remove this...
				CudaDeviceList *devs;
				devs = result;
				printf("Test result: %s\n",  devs->device[0]->name);
				cuda_device__get_packed_size(devs->device[0]);
				// -- /
				break;
		}
			
		if (result != NULL) {
			printf("Sending result\n");
			msg_length = encode_message(&msg, resp_type, result);
			send_message(client_sock_fd, msg, msg_length);
			
			if (result != cuda_dev_array)
				free(result);
			
			result = NULL;
		}
		printf("--------------\nMessage processed, cleaning up...\n");
		if (msg != NULL) {
			free(msg);
			msg = NULL;
		}
		sleep(2); // just for testing...
		close(client_sock_fd);
	}

	return EXIT_FAILURE;
}
