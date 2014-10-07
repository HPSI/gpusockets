#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

char *server_ip;
char *server_port;

inline void *malloc_safe_f(size_t size, const char *file, const int line) {
	void *ptr = NULL;

	ptr = malloc(size);
	if (ptr == NULL && size != 0) {
		fprintf(stderr, "[%s, %i] Memory allocation failed!\n", file, line);
		exit(EXIT_FAILURE);
	}

	return ptr;
}

inline void *realloc_safe_f(void *orig_ptr, size_t size, const char *file, const int line) {
	void *ptr = NULL;

	ptr = realloc(orig_ptr, size);
	if (ptr == NULL && size != 0) {
		fprintf(stderr, "[%s, %i] Memory reallocation failed!\n", file, line);
		exit(EXIT_FAILURE);
	}

	return ptr;
}

inline void *calloc_safe_f(size_t nmemb, size_t size, const char *file, const int line) {
	void *ptr = NULL;

	ptr = calloc(nmemb, size);
	if (ptr == NULL && size != 0) {
		fprintf(stderr, "[%s, %i] Memory allocation failed!\n", file, line);
		exit(EXIT_FAILURE);
	}

	return ptr;
}

int get_server_ip(char *server_ip, char *server_port) {
	const char *gs_server = getenv("GPUSOCK_SERVER"),
		  *gs_port = getenv("GPUSOCK_PORT");

	if (gs_server == NULL) {
		gs_server = DEFAULT_SERVER_IP;
		gdprintf("GPUSOCK_SERVER not defined, using default server ip: %s\n", gs_server);
	}
	if (gs_port == NULL) {
		gs_port = DEFAULT_SERVER_PORT;
		gdprintf("GPUSOCK_PORT not defined, using default server port: %s\n", gs_port);
	}

	sprintf(server_ip, "%s", gs_server);
	sprintf(server_port, "%s", gs_port);

	if (server_ip[0] != '\0' && server_port[0] != '\0')
		return 0;
	else if (server_port[0] != '\0')
		return 1;
	else if (server_ip[0] != '\0')
		return 2;
	else
		return 3;
}
