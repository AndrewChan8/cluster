#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int handle_get_response(struct message *msg, uint32_t request_id, const char *key) {
  if (msg->request_id != request_id) {
    fprintf(stderr, "GET: expected request_id %u, got %u\n",
            request_id, msg->request_id);
    return -1;
  }

  if (msg->type == MSG_GET_RESPONSE) {
    printf("GET value for %s: %.*s\n", key, (int) msg->length, msg->payload);
    return 0;
  }

  if (msg->type == MSG_ERROR) {
    printf("GET error for %s: %.*s\n", key, (int) msg->length, msg->payload);
    return 0;
  }

  fprintf(stderr, "GET: unexpected response type %u\n", msg->type);
  return -1;
}

static int handle_ok_error_response(struct message *msg, uint32_t request_id, const char *op_name) {
  if (msg->request_id != request_id) {
    fprintf(stderr, "%s: expected request_id %u, got %u\n",
            op_name, request_id, msg->request_id);
    return -1;
  }

  if (msg->type == MSG_OK) {
    printf("%s succeeded\n", op_name);
    return 0;
  }

  if (msg->type == MSG_ERROR) {
    printf("%s error: %.*s\n", op_name, (int) msg->length, msg->payload);
    return 0;
  }

  fprintf(stderr, "%s: unexpected response type %u\n", op_name, msg->type);
  return -1;
}

int main(int argc, char *argv[]) {
  const char *host;
  const char *port;
  const char *op;
  const char *key;
  const char *value = NULL;

  int sockfd;
  uint8_t *payload = NULL;
  uint32_t payload_length = 0;
  uint32_t request_id = 1;
  uint32_t msg_type;
  struct message msg;
  int rc = EXIT_FAILURE;

  if (argc < 5) {
    fprintf(stderr,
            "Usage:\n"
            "  %s <host> <port> get <key>\n"
            "  %s <host> <port> put <key> <value>\n"
            "  %s <host> <port> delete <key>\n",
            argv[0], argv[0], argv[0]);
    return EXIT_FAILURE;
  }

  host = argv[1];
  port = argv[2];
  op = argv[3];
  key = argv[4];

  if (strcmp(op, "get") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> get <key>\n", argv[0]);
      return EXIT_FAILURE;
    }

    if (kv_build_get_payload(key, &payload, &payload_length) < 0) {
      die("kv_build_get_payload");
    }

    msg_type = MSG_GET;
  } else if (strcmp(op, "put") == 0) {
    if (argc != 6) {
      fprintf(stderr, "Usage: %s <host> <port> put <key> <value>\n", argv[0]);
      return EXIT_FAILURE;
    }

    value = argv[5];

    if (kv_build_put_payload(key, value, &payload, &payload_length) < 0) {
      die("kv_build_put_payload");
    }

    msg_type = MSG_PUT;
  } else if (strcmp(op, "delete") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Usage: %s <host> <port> delete <key>\n", argv[0]);
      return EXIT_FAILURE;
    }

    if (kv_build_get_payload(key, &payload, &payload_length) < 0) {
      die("kv_build_get_payload");
    }

    msg_type = MSG_DELETE;
  } else {
    fprintf(stderr, "Unknown operation: %s\n", op);
    return EXIT_FAILURE;
  }

  sockfd = connect_to_server(host, port);

  if (send_message(sockfd, msg_type, request_id, payload, payload_length) < 0) {
    free(payload);
    close(sockfd);
    die("send_message");
  }

  free(payload);
  payload = NULL;

  if (recv_message(sockfd, &msg) < 0) {
    close(sockfd);
    die("recv_message");
  }

  printf("Response: type=%u request_id=%u length=%u\n",
         msg.type, msg.request_id, msg.length);

  if (msg_type == MSG_GET) {
    if (handle_get_response(&msg, request_id, key) == 0) {
      rc = EXIT_SUCCESS;
    }
  } else if (msg_type == MSG_PUT) {
    if (handle_ok_error_response(&msg, request_id, "PUT") == 0) {
      rc = EXIT_SUCCESS;
    }
  } else if (msg_type == MSG_DELETE) {
    if (handle_ok_error_response(&msg, request_id, "DELETE") == 0) {
      rc = EXIT_SUCCESS;
    }
  }

  free_message(&msg);
  close(sockfd);
  return rc;
}