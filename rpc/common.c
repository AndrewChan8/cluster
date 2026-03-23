#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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

  if (sockfd == -1) die("connect/socket");
  return sockfd;
}

ssize_t read_n(int fd, void *buf, size_t n) {
  size_t total = 0;
  char *p = buf;

  while (total < n) {
    ssize_t r = read(fd, p + total, n - total);

    if (r == 0) return total;

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