#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int sockfd = connect_to_server(argv[1], argv[2]);

  if (send_message(sockfd, MSG_PING, 1, NULL, 0) < 0) {
    die("send_message");
  }

  struct message msg;
  if (recv_message(sockfd, &msg) < 0) {
    die("recv_message");
  }

  printf("Received message: type=%u request_id=%u length=%u\n",
         msg.type, msg.request_id, msg.length);

  if (msg.type != MSG_PONG) {
    fprintf(stderr, "Expected MSG_PONG, got %u\n", msg.type);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  if (msg.request_id != 1) {
    fprintf(stderr, "Expected request_id 1, got %u\n", msg.request_id);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  if (msg.length != 0) {
    fprintf(stderr, "Expected zero-length payload, got %u\n", msg.length);
    free_message(&msg);
    close(sockfd);
    return EXIT_FAILURE;
  }

  printf("Valid PONG received\n");

  free_message(&msg);
  close(sockfd);
  return EXIT_SUCCESS;
}