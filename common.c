#include <stdio.h>
#include <stdlib.h>

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

int get_server_ip(char **server_ip, char **server_port)
{

   sprintf(server_ip, "%s", getenv("GPUSOCK_SERVER"));
   sprintf(server_port, "%s", getenv("GPUSOCK_PORT"));

   printf("SERVER_IP:%s, %s\n", getenv("GPUSOCK_SERVER"), server_ip);
   printf("SERVER_PORT:%s, %d\n", getenv("GPUSOCK_PORT"), atoi(server_port));

   if (*server_ip == NULL || *server_port == NULL)
	return 1;
   else
	return 0;
}
