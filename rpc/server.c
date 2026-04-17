#include "common.h"

#include <stdio.h>
#include <stdlib.h>
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

    struct message msg;
    if (recv_message(client_fd, &msg) < 0) {
      perror("recv_message");
      close(client_fd);
      continue;
    }

    printf("Received message: type=%u request_id=%u length=%u\n", msg.type, msg.request_id, msg.length);

    if (msg.type == MSG_PING) {
      if (send_message(client_fd, MSG_PONG, msg.request_id, NULL, 0) < 0) {
        perror("send_message");
      }
    } else if (msg.type == MSG_ECHO) {
      if (send_message(client_fd, MSG_ECHO, msg.request_id, msg.payload, msg.length) < 0) {
        perror("send_message");
      }
    } else {
      fprintf(stderr, "Unknown message type: %u\n", msg.type);
    }

    free_message(&msg);
    close(client_fd);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}