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

  const char *payload = "hello";
  uint32_t request_id = 2;
  uint32_t length = (uint32_t) strlen(payload);

  if (send_message(sockfd, MSG_ECHO, request_id, payload, length) < 0) {
    die("send_message");
  }

  struct message msg;
  if (recv_message(sockfd, &msg) < 0) {
    die("recv_message");
  }

  printf("Received message: type=%u request_id=%u length=%u\n",
         msg.type, msg.request_id, msg.length);

  if (msg.type != MSG_ECHO) {
    fprintf(stderr, "Expected MSG_ECHO, got %u\n", msg.type);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  if (msg.request_id != request_id) {
    fprintf(stderr, "Expected request_id %u, got %u\n",
            request_id, msg.request_id);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  if (msg.length != length) {
    fprintf(stderr, "Expected length %u, got %u\n", length, msg.length);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  if (memcmp(msg.payload, payload, length) != 0) {
    fprintf(stderr, "Payload mismatch\n");
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  printf("Valid ECHO received: %.*s\n", (int) msg.length, msg.payload);

  free_message(&msg);
  close(sockfd);
  return EXIT_SUCCESS;
}