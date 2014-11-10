#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
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
#include "timer.h"

void *free_list=NULL, *busy_list=NULL;
client_node *client_list=NULL;

void *connection_handler(void *socket_desc) {
	int client_sock_fd = *(int *) socket_desc, client_id = -1;
	int msg_type, resp_type, arg_cnt;
	void *msg=NULL, *payload=NULL, *result=NULL, *dec_msg=NULL;
	client_node *client_handle=NULL;
	uint32_t msg_length;
	gs_timer mt, rt, dt, pt;

	pl_rd1a = 0;
	pl_rd2a = 0;
	TIMER_RESET(&mt);
	TIMER_RESET(&rt);
	TIMER_RESET(&dt);
	TIMER_RESET(&pt);
	TIMER_RESET(&pl_rds);
	TIMER_RESET(&pl_rdm);
	TIMER_RESET(&pl_rd1);
	TIMER_RESET(&pl_rd2);
	TIMER_RESET(&pl_m);
	TIMER_RESET(&pl_re);
	TIMER_RESET(&ps_i);
	TIMER_RESET(&ps_dg);
	TIMER_RESET(&ps_dgc);
	TIMER_RESET(&ps_dgca);
	TIMER_RESET(&ps_dgn);
	TIMER_RESET(&ps_dgna);
	TIMER_RESET(&ps_cc);
	TIMER_RESET(&ps_cca);
	TIMER_RESET(&ps_cd);
	TIMER_RESET(&ps_cda);
	TIMER_RESET(&ps_ml);
	TIMER_RESET(&ps_mla);
	TIMER_RESET(&ps_mgf);
	TIMER_RESET(&ps_mgfa);
	TIMER_RESET(&ps_ma);
	TIMER_RESET(&ps_maa);
	TIMER_RESET(&ps_mf);
	TIMER_RESET(&ps_mfa);
	TIMER_RESET(&ps_mhd);
	TIMER_RESET(&ps_mhda);
	TIMER_RESET(&ps_mdh);
	TIMER_RESET(&ps_mdha);
	TIMER_RESET(&ps_lk);
	TIMER_RESET(&ps_lka);
	TIMER_RESET(&ps_ext);

	for (;;) {
		TIMER_START(&mt);
		TIMER_START(&rt);
		msg_length = receive_message(&msg, client_sock_fd);
		TIMER_STOP(&rt);
		TIMER_START(&dt);
		if (msg_length > 0)
			msg_type = decode_message(&dec_msg, &payload,
						  msg, msg_length);

		TIMER_STOP(&dt);
		gdprintf("Processing message\n");
		TIMER_START(&pt);
		switch (msg_type) {
			case CUDA_CMD:
				arg_cnt = process_cuda_cmd(&result,
						payload, free_list,
						busy_list, &client_list,
						&client_handle);
				cuCtxSynchronize();
				resp_type = CUDA_CMD_RESULT;
				break;
			case CUDA_DEVICE_QUERY:
				process_cuda_device_query(&result,
						free_list, busy_list);
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
		TIMER_STOP(&pt);

		if (resp_type != -1) {
			gdprintf("Sending result\n");
			pack_cuda_cmd(&payload, result, arg_cnt,
				      CUDA_CMD_RESULT);
			msg_length = encode_message(&msg, resp_type, payload);
			send_message(client_sock_fd, msg, msg_length);

			if (result != NULL) {
				// should be more freeing here...
				free(result);
				result = NULL;
			}
		}

		gdprintf(">>\nMessage processed, cleaning up...\n<<\n");

		if (msg != NULL) {
			free(msg);
			msg = NULL;
		}

		if (client_id == -1)
			client_id = client_handle->id;

		TIMER_STOP(&mt);
		if (get_client_status(client_handle) == 0) {
			// TODO: freeing
			printf("\n--------------\nClient %d finished.\n", client_id);
			break;
		}
	}
	printf("Client elapsed time (usec): %lf\n\n",
			TIMER_TO_USEC(TIMER_TOTAL(&mt)));

	printf("Message average elapsed time (usec):\n- total: %lf\n"
		   "- receive: %lf\n- decode: %lf\n"
		   "- process: %lf\n- send: %lf\n",
			TIMER_TO_USEC(TIMER_AVG(&mt)), TIMER_TO_USEC(TIMER_AVG(&rt)),
			TIMER_TO_USEC(TIMER_AVG(&dt)), TIMER_TO_USEC(TIMER_AVG(&pt)),
			TIMER_TO_USEC((TIMER_TOTAL(&mt) - TIMER_TOTAL(&rt)
				- TIMER_TOTAL(&dt) - TIMER_TOTAL(&pt))
				/ (double) TIMER_COUNT(&mt)));

	printf("\nReceive average elapsed time (usec):\n- malloc: %lf\n"
		   "- read size: %lf\n  |read socket:\n  |- total: %lf\n"
		   "  |- actual: %lf\n  |- waiting: %lf\n- realloc: %lf\n"
		   "- read message: %lf\n  |read socket:\n  |- total: %lf\n"
		   "  |- actual: %lf\n  |- waiting: %lf\n",
			TIMER_TO_USEC(TIMER_AVG(&pl_m)),
			TIMER_TO_USEC(TIMER_AVG(&pl_rds)),
			TIMER_TO_USEC(TIMER_AVG(&pl_rd1)),
			TIMER_TO_USEC(pl_rd1a / (double) TIMER_COUNT(&pl_rd1)),
			TIMER_TO_USEC((TIMER_TOTAL(&pl_rd1) - pl_rd1a)
				/ (double) TIMER_COUNT(&pl_rd1)),
			TIMER_TO_USEC(TIMER_AVG(&pl_re)),
			TIMER_TO_USEC(TIMER_AVG(&pl_rdm)),
			TIMER_TO_USEC(TIMER_AVG(&pl_rd2)),
			TIMER_TO_USEC(pl_rd2a / (double) TIMER_COUNT(&pl_rd2)),
			TIMER_TO_USEC((TIMER_TOTAL(&pl_rd2) - pl_rd2a)
				/ (double) TIMER_COUNT(&pl_rd2)));

	printf("Process average elapsed time (usec):\n"
		   "- init: %lf\n- device_get: %lf\n"
		   "- device_get_count:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- device_get_name:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- context_create:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- context_destroy:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- module_load:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- module_get_function:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- memory_allocate:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- memory_free:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- memcpy_h_to_d:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- memcpy_d_to_h:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- launch_kernel:\n  |- total: %lf\n  |- actual: %lf\n"
		   "- extra: %lf\n\n",
			TIMER_TO_USEC(TIMER_AVG(&ps_i)),
			TIMER_TO_USEC(TIMER_AVG(&ps_dg)),
			TIMER_TO_USEC(TIMER_AVG(&ps_dgc)),
			TIMER_TO_USEC(TIMER_AVG(&ps_dgca)),
			TIMER_TO_USEC(TIMER_AVG(&ps_dgn)),
			TIMER_TO_USEC(TIMER_AVG(&ps_dgna)),
			TIMER_TO_USEC(TIMER_AVG(&ps_cc)),
			TIMER_TO_USEC(TIMER_AVG(&ps_cca)),
			TIMER_TO_USEC(TIMER_AVG(&ps_cd)),
			TIMER_TO_USEC(TIMER_AVG(&ps_cda)),
			TIMER_TO_USEC(TIMER_AVG(&ps_ml)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mla)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mgf)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mgfa)),
			TIMER_TO_USEC(TIMER_AVG(&ps_ma)),
			TIMER_TO_USEC(TIMER_AVG(&ps_maa)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mf)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mfa)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mhd)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mhda)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mdh)),
			TIMER_TO_USEC(TIMER_AVG(&ps_mdha)),
			TIMER_TO_USEC(TIMER_AVG(&ps_lk)),
			TIMER_TO_USEC(TIMER_AVG(&ps_lka)),
			TIMER_TO_USEC(TIMER_AVG(&ps_ext)));

}


int init_server_net(const char *port, struct addrinfo *addr) {
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
	int server_sock_fd, client_sock_fd, msg_type, resp_type, arg_cnt, *new_sock;
	struct sockaddr_in client_addr;
	struct addrinfo local_addr;
	char server_ip[16] /* IPv4 */, server_port[6], *local_port,
		 client_host[NI_MAXHOST], client_serv[NI_MAXSERV];
	socklen_t s;
	void *msg=NULL, *payload=NULL, *result=NULL, *dec_msg=NULL;
	uint32_t msg_length;
	pthread_t sniffer_thread;

	if (argc > 2) {
		printf("Usage: server <local_port>\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 1) {
		printf("No port defined, trying env vars\n");
		if (get_server_ip(server_ip, server_port) < 2) {
			local_port = server_port;
		} else {
			printf("Could not get env vars, using default %s\n", DEFAULT_SERVER_PORT);
			local_port = (char *) DEFAULT_SERVER_PORT;
		}
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

		new_sock = malloc_safe(sizeof(*new_sock));
		*new_sock = client_sock_fd;
		if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *)new_sock) < 0) {
			fprintf(stderr, "could not create thread\n");
			return 1;
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
