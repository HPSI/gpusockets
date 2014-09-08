#include <stdio.h>
#include <stdlib.h>

#include "common.h"

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
