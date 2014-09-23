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
#include "client.h"


int main(int argc, char *argv[]) {
	int client_sock_fd, test_arg = 10, test_res = 0;
	size_t buf_size, file_size;
	char *server_port;
	struct addrinfo server_addr;
	char *server_ip, *a = "Hello", *b = "world";
	uint64_t dev_ptr, ctx_ptr, mod_ptr, func_ptr, ptr1, ptr2, ptr3;
	CudaCmd cmd1 = CUDA_CMD__INIT, cmd2 = CUDA_CMD__INIT,
			cmd3 = CUDA_CMD__INIT, cmd4 = CUDA_CMD__INIT,
			cmd5 = CUDA_CMD__INIT, cmd6 = CUDA_CMD__INIT,
			cmd7 = CUDA_CMD__INIT, cmd8 = CUDA_CMD__INIT,
			cmd9 = CUDA_CMD__INIT, cmd10 = CUDA_CMD__INIT,
			cmd11 = CUDA_CMD__INIT, cmd12 = CUDA_CMD__INIT,
			cmd13 = CUDA_CMD__INIT, cmd14 = CUDA_CMD__INIT,
			cmd15 = CUDA_CMD__INIT;
	void *buffer = NULL, *file = NULL, *result = NULL; 

	if (argc > 3) {
		printf("Usage: client <server_ip> <server_port>\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 1) {
		printf("No server ip or port defined, using defaults %s:%s\n", SERVER_IP, SERVER_PORT);
		server_ip = (char *) SERVER_IP;
		server_port = (char *) SERVER_PORT;
	} else if (argc == 2) {
		printf("No server port defined, using default %s\n", SERVER_PORT);
		server_ip = argv[1];
		server_port = (char *) SERVER_PORT;
	} else {
		server_ip = argv[1];
		server_port = argv[2];
	}

	client_sock_fd = init_client(server_ip, server_port, &server_addr);	
	printf("Connected to server %s on port %s...\n", server_ip, server_port);

	/**
	 * cmd1
	 **/
	printf("\n* DEVICE_GET\n");
	cmd1.type = DEVICE_GET;
	cmd1.arg_count = 1;
	cmd1.n_int_args = 1;
	cmd1.int_args = malloc_safe(sizeof(*(cmd1.int_args)) * cmd1.n_int_args);
	cmd1.int_args[0] = 0;
	/*
	  cmd1.int_args[1] = 2;
	  cmd1.n_str_args = 2;
	  cmd1.str_args = malloc_safe(sizeof(char *) * cmd1.n_str_args);
	  cmd1.str_args[0] = a;
	  cmd1.str_args[1] = b;
	*/
	buf_size = encode_message(&buffer, CUDA_CMD, &cmd1);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd1.int_args);
	free(buffer);	
	get_cuda_cmd_result(&result, client_sock_fd);
	dev_ptr = *(uint64_t *) result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd2
	 **/
	printf("\n* CONTEXT_CREATE\n");
	cmd2.type = CONTEXT_CREATE;
	cmd2.arg_count = 2;
	cmd2.n_uint_args = 2;
	cmd2.uint_args = malloc_safe(sizeof(*(cmd2.uint_args)) * cmd2.n_uint_args);
	cmd2.uint_args[0] = 0;
	cmd2.uint_args[1] = dev_ptr;
	buf_size = encode_message(&buffer, CUDA_CMD, &cmd2);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd2.uint_args);
	get_cuda_cmd_result(&result, client_sock_fd);
	ctx_ptr = *(uint64_t *)result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd3
	 **/	
	printf("\n* MODULE_LOAD\n");
	cmd3.type = MODULE_LOAD;
	cmd3.arg_count = 1;
	cmd3.n_extra_args = 1;
	cmd3.extra_args = malloc_safe(sizeof(*(cmd3.extra_args)) * cmd3.n_extra_args);

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
	mod_ptr = *(uint64_t *) result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd4
	 **/	
	printf("\n* MODULE_GET_FUNCTION\n");
	cmd4.type = MODULE_GET_FUNCTION;
	cmd4.arg_count = 2;
	cmd4.n_uint_args = 1;
	cmd4.uint_args = malloc_safe(sizeof(*(cmd4.uint_args)) * cmd4.n_uint_args);
	cmd4.uint_args[0] = mod_ptr;
	cmd4.n_str_args = 1;
	cmd4.str_args = malloc_safe(sizeof(char *) * cmd4.n_str_args);
	cmd4.str_args[0] = "matSum";

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd4);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd4.uint_args);
	free(cmd4.str_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	func_ptr = *(uint64_t *)result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd5
	 **/	
	printf("\n* MEMORY_ALLOCATE 1\n");
	cmd5.type = MEMORY_ALLOCATE;
	cmd5.arg_count = 1;
	cmd5.n_uint_args =1;
	cmd5.uint_args = malloc_safe(sizeof(*(cmd5.uint_args)) * cmd5.n_uint_args);
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
	printf("\n* MEMORY_ALLOCATE 2\n");
	cmd6.type = MEMORY_ALLOCATE;
	cmd6.arg_count = 1;
	cmd6.n_uint_args =1;
	cmd6.uint_args = malloc_safe(sizeof(*(cmd6.uint_args)) * cmd6.n_uint_args);
	cmd6.uint_args[0] = sizeof(int);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd6);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd6.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	ptr2 = *(uint64_t *)result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd7
	 **/	
	printf("\n* MEMORY_ALLOCATE 3\n");
	cmd7.type = MEMORY_ALLOCATE;
	cmd7.arg_count = 1;
	cmd7.n_uint_args =1;
	cmd7.uint_args = malloc_safe(sizeof(*(cmd7.uint_args)) * cmd7.n_uint_args);
	cmd7.uint_args[0] = sizeof(int);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd7);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd7.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	ptr3 = *(uint64_t *)result;
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd8
	 **/	
	printf("\n* MEMCPY_HOST_TO_DEV 1\n");
	cmd8.type = MEMCPY_HOST_TO_DEV;
	cmd8.arg_count = 2;
	cmd8.n_extra_args = 1;
	cmd8.extra_args = malloc_safe(sizeof(*(cmd8.extra_args)) * cmd8.n_extra_args);
	cmd8.extra_args[0].data = (void *)&test_arg;
	cmd8.extra_args[0].len = sizeof(int);

	cmd8.n_uint_args =1;
	cmd8.uint_args = malloc_safe(sizeof(*(cmd8.uint_args)) * cmd8.n_uint_args);
	cmd8.uint_args[0] = ptr1;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd8);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd8.extra_args);
	free(cmd8.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd9
	 **/	
	printf("\n* MEMCPY_HOST_TO_DEV 2\n");
	cmd9.type = MEMCPY_HOST_TO_DEV;
	cmd9.arg_count = 2;
	cmd9.n_extra_args = 1;
	cmd9.extra_args = malloc_safe(sizeof(*(cmd9.extra_args)) * cmd9.n_extra_args);
	cmd9.extra_args[0].data = (void *)&test_arg;
	cmd9.extra_args[0].len = sizeof(int);

	cmd9.n_uint_args =1;
	cmd9.uint_args = malloc_safe(sizeof(*(cmd9.uint_args)) * cmd9.n_uint_args);
	cmd9.uint_args[0] = ptr2;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd9);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd9.extra_args);
	free(cmd9.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd10
	 **/	
	printf("\n* LAUNCH_KERNEL\n");
	cmd10.type = LAUNCH_KERNEL;
	cmd10.arg_count = 12;
	cmd10.n_uint_args = 12;
	cmd10.uint_args = malloc_safe(sizeof(*(cmd10.uint_args)) * cmd10.n_uint_args);
	// blocks
	cmd10.uint_args[0] = 1;
	cmd10.uint_args[1] = 1;
	cmd10.uint_args[2] = 1;
	// threads
	cmd10.uint_args[3] = 1;
	cmd10.uint_args[4] = 1;
	cmd10.uint_args[5] = 1;
	// shared mem size
	cmd10.uint_args[6] = 0;
	// function
	cmd10.uint_args[7] = func_ptr;
	// stream
	cmd10.uint_args[8] = 0;
	// params
	cmd10.uint_args[9] = ptr1;
	cmd10.uint_args[10] = ptr2;
	cmd10.uint_args[11] = ptr3;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd10);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd10.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd11
	 **/	
	printf("\n* MEMCPY_DEV_TO_HOST\n");
	cmd11.type = MEMCPY_DEV_TO_HOST;
	cmd11.arg_count = 2;
	cmd11.n_uint_args = 2;
	cmd11.uint_args = malloc_safe(sizeof(*(cmd11.uint_args)) * cmd11.n_uint_args);
	cmd11.uint_args[0] = ptr3;
	cmd11.uint_args[1] = sizeof(int);

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd11);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd11.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	test_res = *(int *) result;
	printf("\nExecution result: %d\n\n", test_res);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd12
	 **/	
	printf("\n* MEMORY_FREE 1\n");
	cmd12.type = MEMORY_FREE;
	cmd12.arg_count = 1;
	cmd12.n_uint_args =1;
	cmd12.uint_args = malloc_safe(sizeof(*(cmd12.uint_args)) * cmd12.n_uint_args);
	cmd12.uint_args[0] = ptr1;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd12);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd12.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd13
	 **/	
	printf("\n* MEMORY_FREE 2\n");
	cmd13.type = MEMORY_FREE;
	cmd13.arg_count = 1;
	cmd13.n_uint_args =1;
	cmd13.uint_args = malloc_safe(sizeof(*(cmd13.uint_args)) * cmd13.n_uint_args);
	cmd13.uint_args[0] = ptr2;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd13);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd13.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd14
	 **/	
	printf("\n* MEMORY_FREE 3\n");
	cmd14.type = MEMORY_FREE;
	cmd14.arg_count = 1;
	cmd14.n_uint_args =1;
	cmd14.uint_args = malloc_safe(sizeof(*(cmd14.uint_args)) * cmd14.n_uint_args);
	cmd14.uint_args[0] = ptr3;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd14);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd14.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	// --
	close(client_sock_fd);
	client_sock_fd = init_client(server_ip, server_port, &server_addr);	

	/**
	 * cmd15
	 **/	
	printf("\n* CONTEXT_DESTROY\n");
	cmd15.type = CONTEXT_DESTROY;
	cmd15.arg_count = 1;
	cmd15.n_uint_args = 1;
	cmd15.uint_args = malloc_safe(sizeof(*(cmd15.uint_args)) * cmd15.n_uint_args);
	cmd15.uint_args[0] = ctx_ptr;

	buf_size = encode_message(&buffer, CUDA_CMD, &cmd15);
	send_message(client_sock_fd, buffer, buf_size);

	free(cmd15.uint_args);
	free(buffer);
	get_cuda_cmd_result(&result, client_sock_fd);
	//get_available_gpus(client_sock_fd);

	printf("Message sent succesfully\n");
	close(client_sock_fd);

	return 0;
}
