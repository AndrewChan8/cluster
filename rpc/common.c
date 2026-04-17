#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

int create_server_socket(const char *port) {
  struct addrinfo hints, *res, *p;
  int sockfd = -1;
  int yes = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int rv = getaddrinfo(NULL, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  for (p = res; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) continue;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
      close(sockfd);
      freeaddrinfo(res);
      die("setsockopt");
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;

    close(sockfd);
    sockfd = -1;
  }

  freeaddrinfo(res);

  if (sockfd == -1) die("bind/socket");
  if (listen(sockfd, BACKLOG) == -1) die("listen");

  return sockfd;
}

int connect_to_server(const char *host, const char *port) {
  struct addrinfo hints, *res, *p;
  int sockfd = -1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int rv = getaddrinfo(host, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  for (p = res; p != NULL; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) continue;

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;

    close(sockfd);
    sockfd = -1;
  }

  freeaddrinfo(res);

  if (sockfd == -1) {
    return -1;
  }

  return sockfd;
}

ssize_t read_n(int fd, void *buf, size_t n) {
  size_t total = 0;
  char *p = buf;

  while (total < n) {
    ssize_t r = read(fd, p + total, n - total);

    if (r == 0) return (ssize_t) total;

    if (r < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    total += (size_t) r;
  }

  return (ssize_t) total;
}

ssize_t write_n(int fd, const void *buf, size_t n) {
  size_t total = 0;
  const char *p = buf;

  while (total < n) {
    ssize_t w = write(fd, p + total, n - total);

    if (w < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    total += (size_t) w;
  }

  return (ssize_t) total;
}

int send_message(int fd, uint32_t type, uint32_t request_id, const void *payload, uint32_t length) {
  uint32_t net_type = htonl(type);
  uint32_t net_request_id = htonl(request_id);
  uint32_t net_length = htonl(length);

  if (write_n(fd, &net_type, sizeof(net_type)) != (ssize_t) sizeof(net_type)) {
    return -1;
  }

  if (write_n(fd, &net_request_id, sizeof(net_request_id)) !=
      (ssize_t) sizeof(net_request_id)) {
    return -1;
  }

  if (write_n(fd, &net_length, sizeof(net_length)) != (ssize_t) sizeof(net_length)) {
    return -1;
  }

  if (length > 0) {
    if (payload == NULL) {
      errno = EINVAL;
      return -1;
    }

    if (write_n(fd, payload, length) != (ssize_t) length) {
      return -1;
    }
  }

  return 0;
}

int recv_message(int fd, struct message *msg) {
  uint32_t net_type;
  uint32_t net_request_id;
  uint32_t net_length;

  memset(msg, 0, sizeof(*msg));

  if (read_n(fd, &net_type, sizeof(net_type)) != (ssize_t) sizeof(net_type)) {
    return -1;
  }

  if (read_n(fd, &net_request_id, sizeof(net_request_id)) !=
      (ssize_t) sizeof(net_request_id)) {
    return -1;
  }

  if (read_n(fd, &net_length, sizeof(net_length)) != (ssize_t) sizeof(net_length)) {
    return -1;
  }

  msg->type = ntohl(net_type);
  msg->request_id = ntohl(net_request_id);
  msg->length = ntohl(net_length);
  msg->payload = NULL;

  if (msg->length > MAX_PAYLOAD_SIZE) {
    errno = EMSGSIZE;
    return -1;
  }

  if (msg->length > 0) {
    msg->payload = malloc(msg->length);
    if (msg->payload == NULL) {
      return -1;
    }

    if (read_n(fd, msg->payload, msg->length) != (ssize_t) msg->length) {
      free(msg->payload);
      msg->payload = NULL;
      return -1;
    }
  }

  return 0;
}

int kv_build_get_payload(const char *key, uint8_t **payload_out, uint32_t *length_out) {
  uint32_t key_length;
  uint32_t net_key_length;
  uint8_t *payload;

  if (key == NULL || payload_out == NULL || length_out == NULL) {
    errno = EINVAL;
    return -1;
  }

  key_length = (uint32_t) strlen(key);

  if (key_length > MAX_PAYLOAD_SIZE - sizeof(uint32_t)) {
    errno = EMSGSIZE;
    return -1;
  }

  *length_out = sizeof(uint32_t) + key_length;

  payload = malloc(*length_out);
  if (payload == NULL) {
    return -1;
  }

  net_key_length = htonl(key_length);
  memcpy(payload, &net_key_length, sizeof(net_key_length));
  memcpy(payload + sizeof(net_key_length), key, key_length);

  *payload_out = payload;
  return 0;
}

int kv_parse_get_payload(const uint8_t *payload, uint32_t length, char **key_out) {
  uint32_t net_key_length;
  uint32_t key_length;
  char *key;

  if (payload == NULL || key_out == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (length < sizeof(uint32_t)) {
    errno = EINVAL;
    return -1;
  }

  memcpy(&net_key_length, payload, sizeof(net_key_length));
  key_length = ntohl(net_key_length);

  if (length != sizeof(uint32_t) + key_length) {
    errno = EINVAL;
    return -1;
  }

  key = malloc(key_length + 1);
  if (key == NULL) {
    return -1;
  }

  memcpy(key, payload + sizeof(net_key_length), key_length);
  key[key_length] = '\0';

  *key_out = key;
  return 0;
}

int kv_build_put_payload(const char *key, const char *value, uint8_t **payload_out, uint32_t *length_out) {
  uint32_t key_length;
  uint32_t value_length;
  uint32_t net_key_length;
  uint32_t net_value_length;
  uint8_t *payload;
  uint32_t total_length;

  if (key == NULL || value == NULL || payload_out == NULL || length_out == NULL) {
    errno = EINVAL;
    return -1;
  }

  key_length = (uint32_t) strlen(key);
  value_length = (uint32_t) strlen(value);

  if (key_length > MAX_PAYLOAD_SIZE - sizeof(uint32_t) * 2) {
    errno = EMSGSIZE;
    return -1;
  }

  if (value_length > MAX_PAYLOAD_SIZE - sizeof(uint32_t) * 2 - key_length) {
    errno = EMSGSIZE;
    return -1;
  }

  total_length = sizeof(uint32_t) + key_length + sizeof(uint32_t) + value_length;

  payload = malloc(total_length);
  if (payload == NULL) {
    return -1;
  }

  net_key_length = htonl(key_length);
  net_value_length = htonl(value_length);

  memcpy(payload, &net_key_length, sizeof(net_key_length));
  memcpy(payload + sizeof(net_key_length), key, key_length);
  memcpy(payload + sizeof(net_key_length) + key_length,
         &net_value_length, sizeof(net_value_length));
  memcpy(payload + sizeof(net_key_length) + key_length + sizeof(net_value_length),
         value, value_length);

  *payload_out = payload;
  *length_out = total_length;
  return 0;
}

int kv_parse_put_payload(const uint8_t *payload, uint32_t length, char **key_out, char **value_out) {
  uint32_t net_key_length;
  uint32_t net_value_length;
  uint32_t key_length;
  uint32_t value_length;
  uint32_t offset;
  char *key;
  char *value;

  if (payload == NULL || key_out == NULL || value_out == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (length < sizeof(uint32_t)) {
    errno = EINVAL;
    return -1;
  }

  memcpy(&net_key_length, payload, sizeof(net_key_length));
  key_length = ntohl(net_key_length);
  offset = sizeof(uint32_t);

  if (length < offset + key_length + sizeof(uint32_t)) {
    errno = EINVAL;
    return -1;
  }

  key = malloc(key_length + 1);
  if (key == NULL) {
    return -1;
  }

  memcpy(key, payload + offset, key_length);
  key[key_length] = '\0';
  offset += key_length;

  memcpy(&net_value_length, payload + offset, sizeof(net_value_length));
  value_length = ntohl(net_value_length);
  offset += sizeof(uint32_t);

  if (length != sizeof(uint32_t) + key_length + sizeof(uint32_t) + value_length) {
    free(key);
    errno = EINVAL;
    return -1;
  }

  value = malloc(value_length + 1);
  if (value == NULL) {
    free(key);
    return -1;
  }

  memcpy(value, payload + offset, value_length);
  value[value_length] = '\0';

  *key_out = key;
  *value_out = value;
  return 0;
}

void free_message(struct message *msg) {
  free(msg->payload);
  msg->payload = NULL;
  msg->type = 0;
  msg->request_id = 0;
  msg->length = 0;
}