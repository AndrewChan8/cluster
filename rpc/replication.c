#include "replication.h"
#include "common.h"

#include <stdio.h>
#include <unistd.h>

int replicate_put_to_follower(const char *host,
                              const char *port,
                              uint32_t request_id,
                              const uint8_t *payload,
                              uint32_t payload_length) {
  int sockfd;
  struct message response;
  int rc = -1;

  sockfd = connect_to_server(host, port);
  if (sockfd < 0) {
    perror("connect_to_server");
    return -1;
  }

  if (send_message(sockfd, MSG_REPL_PUT, request_id, payload, payload_length) < 0) {
    perror("send_message");
    close(sockfd);
    return -1;
  }

  if (recv_message(sockfd, &response) < 0) {
    perror("recv_message");
    close(sockfd);
    return -1;
  }

  if (response.request_id != request_id) {
    fprintf(stderr, "replication: expected request_id %u, got %u\n",
            request_id, response.request_id);
  } else if (response.type != MSG_OK) {
    fprintf(stderr, "replication: follower %s returned type %u\n",
            host, response.type);
  } else {
    rc = 0;
  }

  free_message(&response);
  close(sockfd);
  return rc;
}