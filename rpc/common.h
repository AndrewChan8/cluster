#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define BACKLOG 16
#define BUF_SIZE 1024
#define MAX_PAYLOAD_SIZE 4096

enum msg_type {
  MSG_PING = 1,
  MSG_PONG = 2,
  MSG_ECHO = 3,
  MSG_PUT = 4,
  MSG_GET = 5,
  MSG_DELETE = 6,
  MSG_OK = 7,
  MSG_GET_RESPONSE = 8,
  MSG_ERROR = 9
};

struct message {
  uint32_t type;
  uint32_t request_id;
  uint32_t length;
  uint8_t *payload;
};

void die(const char *msg);

int create_server_socket(const char *port);
int connect_to_server(const char *host, const char *port);

ssize_t read_n(int fd, void *buf, size_t n);
ssize_t write_n(int fd, const void *buf, size_t n);

int kv_build_get_payload(const char *key, uint8_t **payload_out, uint32_t *length_out);
int kv_parse_get_payload(const uint8_t *payload, uint32_t length, char **key_out);

int send_message(int fd, uint32_t type, uint32_t request_id, const void *payload, uint32_t length);
int recv_message(int fd, struct message *msg);
void free_message(struct message *msg);

#endif