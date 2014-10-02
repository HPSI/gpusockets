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

int init_server_net(char * port, struct addrinfo *addr) {
	int socket_fd, ret;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		// Allow IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	// For wildcard IP address
	hints.ai_protocol = 0;			// Any protocol
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ret = getaddrinfo(NULL, port, &hints, &addr);
	if (ret) {
		fprintf(stderr, "getaddrinfo failed: [%d] %s\n", ret, gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (socket_fd < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	if (bind(socket_fd, (struct sockaddr*)addr->ai_addr, addr->ai_addrlen) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(socket_fd, 10) < 0) {
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	return socket_fd;
}

int init_server(char *port, struct addrinfo *addr, void **free_list, void **busy_list) {
	int socket_fd;

	printf("Initializing server...\n");
	socket_fd = init_server_net(port, addr);
	discover_cuda_devices(free_list, busy_list);

	return socket_fd;
}

int main(int argc, char *argv[]) {
	int server_sock_fd, client_sock_fd, msg_type, resp_type, arg_cnt;
	struct sockaddr_in client_addr;
	struct addrinfo local_addr;
	socklen_t s;
    char *local_port, client_host[NI_MAXHOST], client_serv[NI_MAXSERV];
	void *msg=NULL, *payload=NULL, *result=NULL, *dec_msg=NULL,
		 *free_list=NULL, *busy_list=NULL, *client_list=NULL, *client_handle=NULL;
	uint32_t msg_length;

	if (argc > 2) {
		printf("Usage: server <local_port>\n");
		exit(EXIT_FAILURE);
	}
	
	if (argc == 1) {
		printf("No port defined, using default %s\n", SERVER_PORT);
		local_port = (char *) SERVER_PORT;
	} else {
		local_port = argv[1];
	}
	
	server_sock_fd = init_server(local_port, &local_addr, &free_list, &busy_list);
	print_cuda_devices(free_list, busy_list);
	printf("\nServer listening on port %s for incoming connections...\n", local_port);

	for (;;) {
		resp_type = -1;
		s = sizeof(client_addr);
		client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&client_addr, &s);
		if (client_sock_fd < 0) {
			perror("accept failed");
			exit(EXIT_FAILURE);
		}

		printf("\nConnection accepted ");
		if (getnameinfo((struct sockaddr*)&client_addr, s,
					client_host, sizeof(client_host), client_serv,
					sizeof(client_serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
			printf("from client @%s:%s\n", client_host, client_serv);
		else 
			printf("from unidentified client");

		for(;;) {
			msg_length = receive_message(&msg, client_sock_fd);
			if (msg_length > 0)
				msg_type = decode_message(&dec_msg, &payload, msg, msg_length);

			printf("Processing message\n");
			switch (msg_type) {
				case CUDA_CMD: 
					arg_cnt = process_cuda_cmd(&result, payload, free_list, busy_list, &client_list, &client_handle);
					resp_type = CUDA_CMD_RESULT;
					break;
				case CUDA_DEVICE_QUERY:
					process_cuda_device_query(&result, free_list, busy_list);
					resp_type = CUDA_DEVICE_LIST;
					break;
			}

			print_clients(client_list);
			print_cuda_devices(free_list, busy_list);

			if (msg != NULL) {
				free(msg);
				msg = NULL;
			}
			if (dec_msg != NULL) {
				free_decoded_message(dec_msg);
				dec_msg = NULL;
				// payload should be invalid now
				payload = NULL;
			}

			if (resp_type != -1) {
				gdprintf("Sending result\n");
				pack_cuda_cmd(&payload, result, arg_cnt, CUDA_CMD_RESULT);
				msg_length = encode_message(&msg, resp_type, payload);
				send_message(client_sock_fd, msg, msg_length);
				
				if (result != NULL) {
					// should be more freeing here...
					free(result);
					result = NULL;
				}
			}
			printf(">>\nMessage processed, cleaning up...\n<<\n");
			if (msg != NULL) {
				free(msg);
				msg = NULL;
			}
			
			if (get_client_status(client_handle) == 0) {
				// TODO: freeing
				printf("\n--------------\nClient finished.\n\n");
				break;
			}
		}
	}
	close(client_sock_fd);
	
	if (free_list != NULL)
		free_cdn_list(free_list);

	if (busy_list != NULL)
		free_cdn_list(busy_list);
	
	if (client_list != NULL)
		free_cdn_list(client_list);

	return EXIT_FAILURE;
}
