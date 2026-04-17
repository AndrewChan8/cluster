#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  uint8_t *payload;
  uint32_t payload_length;
  uint32_t request_id = 3;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int sockfd = connect_to_server(argv[1], argv[2]);

  if (kv_build_get_payload("apple", &payload, &payload_length) < 0) {
    die("kv_build_get_payload");
  }

  if (send_message(sockfd, MSG_GET, request_id, payload, payload_length) < 0) {
    free(payload);
    die("send_message");
  }

  free(payload);
  close(sockfd);
  return EXIT_SUCCESS;
}