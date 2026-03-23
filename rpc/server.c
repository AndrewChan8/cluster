#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int listen_fd = create_server_socket(argv[1]);
  printf("Server listening on port %s\n", argv[1]);

  while (1) {

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &addr_len);

    if (client_fd == -1) {
      perror("accept");
      continue;
    }

    char buf[BUF_SIZE];

    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);

    if (n < 0) {
      perror("read");
      close(client_fd);
      continue;
    }

    buf[n] = '\0';

    printf("Received: %s\n", buf);

    const char *reply = "PONG\n";

    if (write_n(client_fd, reply, strlen(reply)) < 0) {
      perror("write");
    }

    close(client_fd);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}