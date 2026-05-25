#include "handlers.h"
#include "ledger.h"
#include "replication.h"

#include <stdio.h>
#include <string.h>

#define CURRENT_TERM 1

static const char *FOLLOWERS[] = {
  "node2",
  "node3"
};

static const int FOLLOWER_COUNT = 2;

int handle_append(int client_fd,
                  const struct message *msg,
                  server_role_t role,
                  const char *port) {
  char tx[MAX_TX_SIZE + 1];

  if (role != ROLE_LEADER) {
    const char *err = "not leader";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (msg->length > MAX_TX_SIZE) {
    const char *err = "transaction too large";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(tx, msg->payload, msg->length);
  tx[msg->length] = '\0';

  printf("APPEND tx=\"%s\"\n", tx);

  for (int i = 0; i < FOLLOWER_COUNT; i++) {
    if (replicate_append_to_follower(FOLLOWERS[i],
                                     port,
                                     msg->request_id,
                                     msg->payload,
                                     msg->length) < 0) {
      char err[128];

      snprintf(err, sizeof(err),
               "replication to %s failed",
               FOLLOWERS[i]);

      send_message(client_fd,
                   MSG_ERROR,
                   msg->request_id,
                   err,
                   (uint32_t) strlen(err));

      return 0;
    }
  }

  if (ledger_append_local(tx, CURRENT_TERM, 1) < 0) {
    const char *err = "ledger full";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  ledger_print();

  if (send_message(client_fd,
                   MSG_OK,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_repl_append(int client_fd,
                       const struct message *msg) {
  char tx[MAX_TX_SIZE + 1];

  if (msg->length > MAX_TX_SIZE) {
    const char *err = "transaction too large";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(tx, msg->payload, msg->length);
  tx[msg->length] = '\0';

  printf("REPL_APPEND tx=\"%s\"\n", tx);

  if (ledger_append_local(tx, CURRENT_TERM, 1) < 0) {
    const char *err = "ledger full";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  ledger_print();

  if (send_message(client_fd,
                   MSG_OK,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_get_log(int client_fd,
                   const struct message *msg) {
  char buf[MAX_PAYLOAD_SIZE];
  int n;

  n = ledger_serialize(buf, sizeof(buf));
  if (n < 0) {
    const char *err = "failed to serialize ledger";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (send_message(client_fd,
                   MSG_LOG_RESPONSE,
                   msg->request_id,
                   buf,
                   (uint32_t) n) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_prepare_append(int client_fd,
                          const struct message *msg) {
  printf("PREPARE_APPEND request_id=%u\n",
         msg->request_id);

  if (send_message(client_fd,
                   MSG_OK,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_commit_append(int client_fd,
                         const struct message *msg) {
  return handle_repl_append(client_fd, msg);
}

int handle_abort(int client_fd,
                 const struct message *msg) {
  printf("ABORT request_id=%u\n",
         msg->request_id);

  if (send_message(client_fd,
                   MSG_OK,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}