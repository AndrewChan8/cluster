#include "common.h"
#include "kv_store.h"
#include "replication.h"
#include "handlers.h"
#include "server_context.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    perror("gethostname");
    return EXIT_FAILURE;
  }
  hostname[sizeof(hostname) - 1] = '\0';

  server_role_t role;
  if (strcmp(hostname, "node1") == 0) {
    role = ROLE_LEADER;
  } else {
    role = ROLE_FOLLOWER;
  }

  int listen_fd = create_server_socket(argv[1]);
  printf("Server listening on port %s as %s (%s)\n",
         argv[1],
         role == ROLE_LEADER ? "LEADER" : "FOLLOWER",
         hostname);

  kv_store_init();

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

    printf("Received message: type=%u request_id=%u length=%u\n",
           msg.type, msg.request_id, msg.length);

    if (msg.type == MSG_PING) {
      if (send_message(client_fd, MSG_PONG, msg.request_id, NULL, 0) < 0) {
        perror("send_message");
      }

    } else if (msg.type == MSG_ECHO) {
      if (send_message(client_fd, MSG_ECHO, msg.request_id, msg.payload, msg.length) < 0) {
        perror("send_message");
      }
    } else if (msg.type == MSG_GET) {
      handle_get(client_fd, &msg);
    } else if (msg.type == MSG_PUT) {
      handle_put(client_fd, &msg, role, argv[1]);
    } else if (msg.type == MSG_REPL_PUT) {
      handle_repl_put(client_fd, &msg);
    } else if (msg.type == MSG_REPL_DELETE) {
      handle_repl_delete(client_fd, &msg);
    } else if (msg.type == MSG_DELETE) {
      handle_delete(client_fd, &msg, role, argv[1]);
    } else {
      fprintf(stderr, "Unknown message type: %u\n", msg.type);
    }

    free_message(&msg);
    close(client_fd);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}