#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "protocol.h"
#include "common.h"
#include "common.pb-c.h"

#if 0
ssize_t read_socket_msg(int fd, void *buffer, size_t bytes) {
	ssize_t	b_read, b_total = 0;
	struct msghdr msghdr;
	size_t iovlen;
	ssize_t copied;

	//do {
	b_read = recvmsg(fd, &msghdr, 0);
	if (b_read < 0) {
		perror("read socket failed");
		exit(EXIT_FAILURE);
	}
	//} while (b_total < bytes);

	//printf("Bytes received: %zd\n", b_total);
	iovlen = msghdr.msg_iovlen;
	while (iovlen--) {
		struct iovec *iov = msghdr.msg_iov;
		memcpy(buffer, iov->iov_base, iov->iov_len);
		copied+=iov->iov_len;
		buffer+=iov->iov_len;
	}

	return copied;
}
#endif

#include "timer.h"
__thread gs_timer pl_rds, pl_rdm, pl_rd1, pl_rd2, pl_m, pl_re;
__thread unsigned long long pl_rd1a, pl_rd2a;
static __thread gs_timer *rdt;
static __thread unsigned long long *rda;

ssize_t read_socket(int fd, void *buffer, size_t bytes) {
	ssize_t	b_read, b_total = 0;
	int flags, err;
	unsigned long long tmp = TIMER_TOTAL(rdt);

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("read socket fcntl failed");
		exit(EXIT_FAILURE);
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("read socket fcntl failed");
		exit(EXIT_FAILURE);
	}

	TIMER_START(rdt);
	do {
		b_read = read(fd, buffer+b_total, bytes-b_total);
		if (b_read < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				TIMER_GET(rdt);
				tmp = TIMER_TOTAL(rdt);
			} else {
				perror("read socket failed");
				exit(EXIT_FAILURE);
			}
		} else {
			b_total += b_read;
		}
	} while (b_total < bytes);
	TIMER_STOP(rdt);

	*rda += TIMER_TOTAL(rdt) - tmp;

	if (fcntl(fd, F_SETFL, flags) < 0) {
		perror("read socket fcntl failed");
		exit(EXIT_FAILURE);
	}

	gdprintf("Bytes received: %zd\n", b_total);
	return b_total;
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

	gdprintf("Bytes sent: %zd\n", b_total);
	return b_total;
}

void send_message(int sock_fd, void *buffer, size_t buf_size) {
	gdprintf("Going to send %zu bytes...\n", buf_size);
	write_socket(sock_fd, buffer, buf_size);
}

uint32_t receive_message(void **enc_msg, int sock_fd) {
	void *buffer;
	uint32_t msg_length;
	int ret = 0;

	TIMER_START(&pl_m);
	buffer = malloc_safe(sizeof(uint32_t));
	TIMER_STOP(&pl_m);

	rdt = &pl_rd1;
	rda = &pl_rd1a;
	TIMER_START(&pl_rds);
	// read message length
	read_socket(sock_fd, buffer, sizeof(uint32_t));
	TIMER_STOP(&pl_rds);

	msg_length = ntohl(*(uint32_t *)buffer);
	gdprintf("Going to read a message of %u bytes...\n", msg_length);

	TIMER_START(&pl_re);
	buffer = realloc(buffer, msg_length);
	TIMER_STOP(&pl_re);

	rdt = &pl_rd2;
	rda = &pl_rd2a;
	TIMER_START(&pl_rdm);
	// read message
	read_socket(sock_fd, buffer, msg_length);
	TIMER_STOP(&pl_rdm);

	*enc_msg = buffer;

	return msg_length;
}

int decode_message(void **result, void **payload, void *enc_msg, uint32_t enc_msg_length) {
	Cookie *msg;

	gdprintf("Decoding message data...\n");
	msg = cookie__unpack(NULL, enc_msg_length, (uint8_t *)enc_msg);
	if (msg == NULL) {
		fprintf(stderr, "message unpacking failed\n");
		return -1;
	}

	switch (msg->type) {
		case CUDA_CMD:
			gdprintf("--------------\nIs CUDA_CMD\n");
			*payload = msg->cuda_cmd;
			break;
		case CUDA_CMD_RESULT:
			gdprintf("--------------\nIs CUDA_CMD_RESULT\n");
			*payload = msg->cuda_cmd;
			break;
		case CUDA_DEVICE_QUERY:
			gdprintf("--------------\nIs CUDA_DEVICE_QUERY\n");
			*payload = NULL;
			break;
		case CUDA_DEVICE_LIST:
			gdprintf("--------------\nIs CUDA_DEVICE_LIST\n");
			*payload = msg->cuda_devices;
			break;
	}

	// We can't call this here unless we make a *deep* copy of the
	// message payload...
	//cookie__free_unpacked(msg, NULL);
	*result = msg;

	return msg->type;
}

size_t encode_message(void **result, int msg_type, void *payload) {
	size_t buf_size;
	uint32_t msg_length, msg_len_n;
	Cookie message = COOKIE__INIT;
	void *buffer, *msg_buffer;

	gdprintf("Encoding message data...\n");
	message.type = msg_type;

	switch (msg_type) {
		case CUDA_CMD:
		case CUDA_CMD_RESULT:
			message.cuda_cmd = payload;
			break;
		case CUDA_DEVICE_QUERY:
			break;
		case CUDA_DEVICE_LIST:
			message.cuda_devices = payload;
			break;
	}

	msg_length = cookie__get_packed_size(&message);
	msg_buffer = malloc_safe(msg_length);
	cookie__pack(&message, msg_buffer);
	msg_len_n = htonl(msg_length);

	buf_size = msg_length + sizeof(msg_len_n);
	buffer = malloc_safe(buf_size);
	memcpy(buffer, &msg_len_n, sizeof(msg_len_n));
	memcpy(buffer+sizeof(msg_len_n), msg_buffer, msg_length);

	*result = buffer;

	return buf_size;
}

void free_decoded_message(void *msg) {
//	Cookie *message = msg;

	gdprintf("Freeing allocated memory for message...\n");
	cookie__free_unpacked((Cookie *) msg, NULL);
}
