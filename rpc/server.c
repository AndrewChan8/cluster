/*
  server.c

  Entry point for each replicated ledger server node.
  The server determines its role from the hostname, initializes the local
  ledger, accepts TCP client connections, receives framed protocol messages,
  and dispatches each message to the appropriate handler.

  All nodes start as followers.
  A leader is selected dynamically through the election module.
*/

#include "common.h"
#include "ledger.h"
#include "replication.h"
#include "handlers.h"
#include "server_context.h"
#include "anti_entropy.h"
#include "election.h"
#include "failure_injector.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

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

  server_context_t ctx;

  memset(&ctx, 0, sizeof(ctx));

  snprintf(ctx.node_id,
          sizeof(ctx.node_id),
          "%s",
          hostname);

  snprintf(ctx.port,
          sizeof(ctx.port),
          "%s",
          argv[1]);
          
  ctx.current_term = 1;
  ctx.last_heartbeat_ms = 0;
  ctx.mode = CONSISTENCY_STRONG;

  ctx.current_leader[0] = '\0';

  ctx.voted_for[0] = '\0';

  ctx.role = ROLE_FOLLOWER;

  if (pthread_mutex_init(&ctx.lock, NULL) != 0) {
    perror("pthread_mutex_init");
    return EXIT_FAILURE;
  }

  failure_injector_init();

  election_init(&ctx);

  int listen_fd = create_server_socket(argv[1]);

  if (start_heartbeat_thread(&ctx) < 0) {
    fprintf(stderr, "failed to start heartbeat thread\n");
    close(listen_fd);
    return EXIT_FAILURE;
  }

  if (start_election_timeout_thread(&ctx) < 0) {
    fprintf(stderr, "failed to start election timeout thread\n");
    close(listen_fd);
    return EXIT_FAILURE;
  }

  printf("Server listening on port %s as %s (%s)\n",
        argv[1],
        ctx.role == ROLE_LEADER ? "LEADER" : "FOLLOWER",
        hostname);

  ledger_init();

  start_anti_entropy_thread(&ctx);

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

    if (msg.type != MSG_HEARTBEAT &&
        msg.type != MSG_STATUS) {
      printf("Received message: type=%u request_id=%u length=%u\n",
            msg.type,
            msg.request_id,
            msg.length);
    }

    if (msg.type == MSG_PING) {
      if (send_message(client_fd, MSG_PONG, msg.request_id, NULL, 0) < 0) {
        perror("send_message");
      }

    } else if (msg.type == MSG_ECHO) {
      if (send_message(client_fd, MSG_ECHO, msg.request_id,
                       msg.payload, msg.length) < 0) {
        perror("send_message");
      }

    } else if (msg.type == MSG_APPEND) {
      handle_append(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_REPL_APPEND) {
      handle_repl_append(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_GET_LOG) {
      handle_get_log(client_fd, &msg);

    } else if (msg.type == MSG_SET_MODE) {
      handle_set_mode(client_fd, &msg, &ctx);
      
    } else if (msg.type == MSG_PREPARE_APPEND) {
      handle_prepare_append(client_fd, &msg);

    } else if (msg.type == MSG_COMMIT_APPEND) {
      handle_commit_append(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_ABORT) {
      handle_abort(client_fd, &msg);

    } else if (msg.type == MSG_SYNC_REQUEST) {
      handle_sync_request(client_fd, &msg);

    } else if (msg.type == MSG_SYNC_RESPONSE) {
      handle_sync_response(client_fd, &msg);
      
    } else if (msg.type == MSG_STATUS) {
      handle_status(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_REPAIR) {
      handle_repair(client_fd, &msg);
    
    } else if (msg.type == MSG_REQUEST_VOTE) {
      handle_request_vote(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_HEARTBEAT) {
      handle_heartbeat(client_fd, &msg, &ctx);

    } else if (msg.type == MSG_INJECT_LATENCY) {
      handle_inject_latency(client_fd, &msg);

    } else if (msg.type == MSG_INJECT_DROP) {
      handle_inject_drop(client_fd, &msg);

    } else {
      fprintf(stderr, "Unknown message type: %u\n", msg.type);
    }

    free_message(&msg);
    close(client_fd);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}