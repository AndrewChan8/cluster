#include "handlers.h"
#include "ledger.h"
#include "replication.h"

#include <stdio.h>
#include <string.h>

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

  if (replicate_append_to_follower("node2",
                                   port,
                                   msg->request_id,
                                   msg->payload,
                                   msg->length) < 0) {
    const char *err = "replication to node2 failed";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (replicate_append_to_follower("node3",
                                   port,
                                   msg->request_id,
                                   msg->payload,
                                   msg->length) < 0) {
    const char *err = "replication to node3 failed";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (ledger_append_local(tx, 1, 1) < 0) {
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

  if (ledger_append_local(tx, 1, 1) < 0) {
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
  (void) client_fd;
  (void) msg;

  ledger_print();
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