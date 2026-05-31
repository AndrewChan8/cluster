/*
  client.c

  Command-line client for interacting with the replicated ledger system.
  Supports append, log, status, consistency mode switching, and manual
  recovery synchronization commands.

  The client is also used by experiment scripts to drive system behavior
  under normal and failure scenarios.
*/

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int handle_response(struct message *msg,
                           uint32_t request_id,
                           const char *op_name) {
  if (msg->request_id != request_id) {
    fprintf(stderr, "%s: expected request_id %u, got %u\n",
            op_name, request_id, msg->request_id);
    return -1;
  }

  if (msg->type == MSG_OK) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_LOG_RESPONSE) {
    printf("%.*s", (int) msg->length, msg->payload);
    return 0;
  }

  if (msg->type == MSG_ERROR) {
    printf("%s error: %.*s\n", op_name, (int) msg->length, msg->payload);
    return 0;
  }

  if (msg->type == MSG_STATUS_RESPONSE) {
    printf("%.*s\n", (int) msg->length, msg->payload);
    return 0;
  }

  if (msg->type == MSG_REPAIR_RESPONSE) {
    printf("%.*s\n", (int) msg->length, msg->payload);
    return 0;
  }

  if (msg->type == MSG_HEARTBEAT_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_REQUEST_VOTE_RESPONSE) {
    printf("vote result: %.*s\n",
          (int) msg->length,
          msg->payload);
    return 0;
  }

  if (msg->type == MSG_INJECT_LATENCY_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_INJECT_DROP_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_INJECT_PARTITION_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_INJECT_HEAL_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_SET_ADAPTIVE_RESPONSE) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  fprintf(stderr, "%s: unexpected response type %u\n", op_name, msg->type);
  return -1;
}

int main(int argc, char *argv[]) {
  const char *host;
  const char *port;
  const char *op;
  const char *tx = NULL;

  int sockfd;
  uint32_t request_id = 1;
  uint32_t msg_type;
  const void *payload = NULL;
  uint32_t payload_length = 0;
  struct message msg;
  int rc = EXIT_FAILURE;

  if (argc != 4 && argc != 5) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <host> <port> append <transaction>\n"
        "  %s <host> <port> log\n"
        "  %s <host> <port> mode <strong|quorum|eventual>\n"
        "  %s <host> <port> sync <leader-host>\n"
        "  %s <host> <port> status\n"
        "  %s <host> <port> repair <leader-host>\n"
        "  %s <host> <port> request_vote\n"
        "  %s <host> <port> heartbeat\n"
        "  %s <host> <port> latency <milliseconds>\n"
        "  %s <host> <port> drop <percent>\n"
        "  %s <host> <port> partition <peer>\n"
        "  %s <host> <port> heal\n"
        "  %s <host> <port> adaptive <on|off>\n",
        argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
    return EXIT_FAILURE;
  }

  host = argv[1];
  port = argv[2];
  op = argv[3];

  if (strcmp(op, "append") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> append <transaction>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_APPEND;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "log") == 0) {
    if (argc != 4) {
      fprintf(stderr, "Usage: %s <host> <port> log\n", argv[0]);
      return EXIT_FAILURE;
    }

    msg_type = MSG_GET_LOG;
    payload = NULL;
    payload_length = 0;

  } else if (strcmp(op, "mode") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> mode <strong|quorum|eventual>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_SET_MODE;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "sync") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> sync <leader-host>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_SYNC_REQUEST;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "status") == 0) {
    if (argc != 4) {
      fprintf(stderr, "Usage: %s <host> <port> status\n", argv[0]);
      return EXIT_FAILURE;
    }

    msg_type = MSG_STATUS;
    payload = NULL;
    payload_length = 0;

  } else if (strcmp(op, "repair") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> repair <leader-host>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_REPAIR;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "request_vote") == 0) {
    if (argc != 4 && argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> request_vote [candidate-id]\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = (argc == 5) ? argv[4] : host;

    msg_type = MSG_REQUEST_VOTE;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);
      
  } else if (strcmp(op, "heartbeat") == 0) {
    if (argc != 4) {
      fprintf(stderr, "Usage: %s <host> <port> heartbeat\n", argv[0]);
      return EXIT_FAILURE;
    }

    msg_type = MSG_HEARTBEAT;
    payload = host;
    payload_length = (uint32_t) strlen(host);

  } else if (strcmp(op, "latency") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> latency <milliseconds>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_INJECT_LATENCY;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "drop") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> drop <percent>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_INJECT_DROP;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "partition") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> partition <peer>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_INJECT_PARTITION;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);

  } else if (strcmp(op, "heal") == 0) {
    if (argc != 4) {
      fprintf(stderr, "Usage: %s <host> <port> heal\n", argv[0]);
      return EXIT_FAILURE;
    }

    msg_type = MSG_INJECT_HEAL;
    payload = NULL;
    payload_length = 0;
    
  } else if (strcmp(op, "adaptive") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> adaptive <on|off>\n", argv[0]);
      return EXIT_FAILURE;
    }

    tx = argv[4];
    msg_type = MSG_SET_ADAPTIVE;
    payload = tx;
    payload_length = (uint32_t) strlen(tx);
    
  } else {
    fprintf(stderr, "Unknown operation: %s\n", op);
    return EXIT_FAILURE;
  }

  sockfd = connect_to_server(host, port);
  if (sockfd < 0) {
    perror("connect_to_server");
    return EXIT_FAILURE;
  }

  if (send_message(sockfd,
                   msg_type,
                   request_id,
                   payload,
                   payload_length) < 0) {
    close(sockfd);
    die("send_message");
  }

  if (recv_message(sockfd, &msg) < 0) {
    close(sockfd);
    die("recv_message");
  }

  printf("Response: type=%u request_id=%u length=%u\n",
         msg.type, msg.request_id, msg.length);

  if (handle_response(&msg, request_id, op) == 0) {
    rc = EXIT_SUCCESS;
  }

  free_message(&msg);
  close(sockfd);
  return rc;
}