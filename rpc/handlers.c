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

static consistency_mode_t current_mode = CONSISTENCY_STRONG;

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

    int ack_count = 0;

  if (current_mode == CONSISTENCY_STRONG) {
    for (int i = 0; i < FOLLOWER_COUNT; i++) {
      if (prepare_append_to_follower(FOLLOWERS[i],
                                     port,
                                     msg->request_id,
                                     msg->payload,
                                     msg->length) == 0) {
        ack_count++;
      } else {
        fprintf(stderr, "prepare to %s failed\n", FOLLOWERS[i]);
      }
    }

    if (ack_count < FOLLOWER_COUNT) {
      for (int i = 0; i < FOLLOWER_COUNT; i++) {
        abort_follower(FOLLOWERS[i], port, msg->request_id);
      }

      const char *err = "strong mode prepare failed";

      send_message(client_fd,
                   MSG_ERROR,
                   msg->request_id,
                   err,
                   (uint32_t) strlen(err));

      return 0;
    }

    for (int i = 0; i < FOLLOWER_COUNT; i++) {
      if (commit_append_to_follower(FOLLOWERS[i],
                                    port,
                                    msg->request_id,
                                    msg->payload,
                                    msg->length) < 0) {
        fprintf(stderr, "commit to %s failed\n", FOLLOWERS[i]);
      }
    }

  } else {
    for (int i = 0; i < FOLLOWER_COUNT; i++) {
      if (replicate_append_to_follower(FOLLOWERS[i],
                                       port,
                                       msg->request_id,
                                       msg->payload,
                                       msg->length) == 0) {
        ack_count++;
      } else {
        fprintf(stderr, "replication to %s failed\n", FOLLOWERS[i]);
      }
    }

    if (current_mode == CONSISTENCY_QUORUM && ack_count < 1) {
      const char *err = "quorum mode requires at least one follower";

      send_message(client_fd,
                   MSG_ERROR,
                   msg->request_id,
                   err,
                   (uint32_t) strlen(err));

      return 0;
    }
  }

  printf("Mode=%d follower_acks=%d/%d\n",
         current_mode,
         ack_count,
         FOLLOWER_COUNT);
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

int handle_set_mode(int client_fd,
                    const struct message *msg,
                    server_role_t role) {
  char mode[32];

  if (role != ROLE_LEADER) {
    const char *err = "not leader";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (msg->length == 0 || msg->length >= sizeof(mode)) {
    const char *err = "invalid mode";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(mode, msg->payload, msg->length);
  mode[msg->length] = '\0';

  if (strcmp(mode, "strong") == 0) {
    current_mode = CONSISTENCY_STRONG;
  } else if (strcmp(mode, "quorum") == 0) {
    current_mode = CONSISTENCY_QUORUM;
  } else if (strcmp(mode, "eventual") == 0) {
    current_mode = CONSISTENCY_EVENTUAL;
  } else {
    const char *err = "unknown mode";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  printf("Consistency mode set to %s\n", mode);

  if (send_message(client_fd,
                   MSG_OK,
                   msg->request_id,
                   NULL,
                   0) < 0) {
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