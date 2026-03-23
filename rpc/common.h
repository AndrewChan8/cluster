#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define BACKLOG 16
#define BUF_SIZE 1024

void die(const char *msg);

int create_server_socket(const char *port);
int connect_to_server(const char *host, const char *port);

ssize_t read_n(int fd, void *buf, size_t n);
ssize_t write_n(int fd, const void *buf, size_t n);

#endif