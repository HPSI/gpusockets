#ifndef PROTOCOL_H
#define PROTOCOL_H

ssize_t read_socket(int fd, void *buffer, size_t bytes);

ssize_t write_socket(int fd, void *buffer, size_t bytes);

void send_message(int sock_fd, void *buffer, size_t buf_size);

uint32_t receive_message(void **enc_msg, int sock_fd);

int decode_message(void **result, void **payload, void *enc_msg, uint32_t msg_length);

size_t encode_message(void **result, int msg_type, void *payload);

void free_decoded_message(void *msg);

#endif /* PROTOCOL_H */
