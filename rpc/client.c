#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int sockfd = connect_to_server(argv[1], argv[2]);

  const char *msg = "PING\n";

  if (write_n(sockfd, msg, strlen(msg)) < 0) {
    die("write");
  }

  char buf[BUF_SIZE];

  ssize_t n = read(sockfd, buf, sizeof(buf) - 1);

  if (n < 0) {
    die("read");
  }

  buf[n] = '\0';

  printf("Server replied: %s", buf);

  close(sockfd);

  return EXIT_SUCCESS;
}