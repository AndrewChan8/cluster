/*
  replication.c

  Implements inter-node communication helpers for distributed replication.
  This module is responsible for:

  - follower append replication
  - strong consistency prepare/commit messaging
  - abort propagation
  - follower recovery synchronization
  - leader snapshot requests

  Replication operations use the shared framed TCP protocol defined in
  common.c/common.h.
*/

#include "replication.h"
#include "common.h"
#include "ledger.h"

#include <stdio.h>
#include <unistd.h>


static int send_to_follower(const char *host,
                            const char *port,
                            uint32_t msg_type,
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

  if (send_message(sockfd, msg_type, request_id, payload, payload_length) < 0) {
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

int replicate_append_to_follower(const char *host,
                                 const char *port,
                                 uint32_t request_id,
                                 const uint8_t *payload,
                                 uint32_t payload_length) {
  return send_to_follower(host,
                          port,
                          MSG_REPL_APPEND,
                          request_id,
                          payload,
                          payload_length);
}

int prepare_append_to_follower(const char *host,
                               const char *port,
                               uint32_t request_id,
                               const uint8_t *payload,
                               uint32_t payload_length) {
  return send_to_follower(host,
                          port,
                          MSG_PREPARE_APPEND,
                          request_id,
                          payload,
                          payload_length);
}

int commit_append_to_follower(const char *host,
                              const char *port,
                              uint32_t request_id,
                              const uint8_t *payload,
                              uint32_t payload_length) {
  return send_to_follower(host,
                          port,
                          MSG_COMMIT_APPEND,
                          request_id,
                          payload,
                          payload_length);
}

int abort_follower(const char *host,
                   const char *port,
                   uint32_t request_id) {
  return send_to_follower(host,
                          port,
                          MSG_ABORT,
                          request_id,
                          NULL,
                          0);
}

int request_sync_from_leader(const char *leader_host,
                             const char *port,
                             uint32_t request_id) {
  int sockfd;
  struct message msg;
  int rc = -1;

  sockfd = connect_to_server(leader_host, port);
  if (sockfd < 0) {
    perror("connect_to_server");
    return -1;
  }

  if (send_message(sockfd,
                   MSG_SYNC_REQUEST,
                   request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
    close(sockfd);
    return -1;
  }

  if (recv_message(sockfd, &msg) < 0) {
    perror("recv_message");
    close(sockfd);
    return -1;
  }

  if (msg.type != MSG_SYNC_RESPONSE) {
    fprintf(stderr,
            "expected MSG_SYNC_RESPONSE, got %u\n",
            msg.type);

    free_message(&msg);
    close(sockfd);
    return -1;
  }

  if (ledger_apply_sync_payload(msg.payload,
                                msg.length) < 0) {
    fprintf(stderr, "failed to apply sync payload\n");

    free_message(&msg);
    close(sockfd);
    return -1;
  }

  printf("Recovered ledger from leader:\n");
  ledger_print();

  rc = 0;

  free_message(&msg);
  close(sockfd);
  return rc;
}