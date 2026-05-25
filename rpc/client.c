#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int handle_ok_error_response(struct message *msg,
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
  const char *tx;

  int sockfd;
  uint32_t request_id = 1;
  struct message msg;
  int rc = EXIT_FAILURE;

  if (argc != 5) {
    fprintf(stderr,
            "Usage:\n"
            "  %s <host> <port> append <transaction>\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  host = argv[1];
  port = argv[2];
  op = argv[3];
  tx = argv[4];

  if (strcmp(op, "append") != 0) {
    fprintf(stderr, "Unknown operation: %s\n", op);
    return EXIT_FAILURE;
  }

  sockfd = connect_to_server(host, port);
  if (sockfd < 0) {
    perror("connect_to_server");
    return EXIT_FAILURE;
  }

  if (send_message(sockfd,
                   MSG_APPEND,
                   request_id,
                   tx,
                   (uint32_t) strlen(tx)) < 0) {
    close(sockfd);
    die("send_message");
  }

  if (recv_message(sockfd, &msg) < 0) {
    close(sockfd);
    die("recv_message");
  }

  printf("Response: type=%u request_id=%u length=%u\n",
         msg.type, msg.request_id, msg.length);

  if (handle_ok_error_response(&msg, request_id, "APPEND") == 0) {
    rc = EXIT_SUCCESS;
  }

  free_message(&msg);
  close(sockfd);
  return rc;
}