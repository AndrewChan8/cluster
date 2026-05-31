/*
  handlers.c

  Implements the primary distributed protocol logic for the replicated ledger.
  This module coordinates:

  - client append requests
  - replication behavior
  - strong/quorum/eventual consistency semantics
  - prepare/commit/abort flows
  - recovery synchronization
  - replica divergence inspection
  - runtime consistency mode switching

  The handler layer acts as the coordination/control plane of the system.
*/

#include "handlers.h"
#include "ledger.h"
#include "replication.h"
#include "failure_injector.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_CONFLICT_KEYS 32
#define MAX_CONFLICT_KEY_SIZE 64

static char recent_write_keys[MAX_CONFLICT_KEYS][MAX_CONFLICT_KEY_SIZE];
static int recent_write_count = 0;

static const char *CLUSTER_NODES[] = {
  "node1",
  "node2",
  "node3"
};

static const int CLUSTER_NODE_COUNT = 3;

static int detect_conflicting_write(const char *tx) {
  char key[MAX_CONFLICT_KEY_SIZE];
  const char *colon;
  size_t key_len;

  colon = strchr(tx, ':');
  if (colon == NULL) {
    return 0;
  }

  key_len = (size_t)(colon - tx);
  if (key_len == 0 || key_len >= sizeof(key)) {
    return 0;
  }

  memcpy(key, tx, key_len);
  key[key_len] = '\0';

  for (int i = 0; i < recent_write_count; i++) {
    if (strcmp(recent_write_keys[i], key) == 0) {
      return 1;
    }
  }

  if (recent_write_count < MAX_CONFLICT_KEYS) {
    snprintf(recent_write_keys[recent_write_count],
             sizeof(recent_write_keys[recent_write_count]),
             "%s",
             key);
    recent_write_count++;
  }

  return 0;
}

int handle_append(int client_fd,
                  const struct message *msg,
                  server_context_t *ctx) {
  char tx[MAX_TX_SIZE + 1];
  int ack_count = 0;
  int follower_count = CLUSTER_NODE_COUNT - 1;
  int conflict_detected = 0;

  if (ctx->role != ROLE_LEADER) {
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

  if (detect_conflicting_write(tx)) {
    conflict_detected = 1;
    printf("CONFLICT DETECTED: tx=\"%s\"\n", tx);

    if (ctx->adaptive_enabled &&
        ctx->mode == CONSISTENCY_EVENTUAL) {
      ctx->mode = CONSISTENCY_QUORUM;
      printf("ADAPTIVE: conflict detected; switching EVENTUAL -> QUORUM\n");
    }
    else if (ctx->adaptive_enabled &&
            ctx->mode == CONSISTENCY_QUORUM) {
      ctx->mode = CONSISTENCY_STRONG;
      printf("ADAPTIVE: conflict detected; switching QUORUM -> STRONG\n");
    }
  }

  if (ctx->mode == CONSISTENCY_STRONG) {

    for (int i = 0; i < CLUSTER_NODE_COUNT; i++) {

      if (strcmp(CLUSTER_NODES[i], ctx->node_id) == 0) {
        continue;
      }

      if (prepare_append_to_follower(CLUSTER_NODES[i],
                                     ctx->port,
                                     msg->request_id,
                                     msg->payload,
                                     msg->length) == 0) {
        ack_count++;
      } else {
        fprintf(stderr,
                "prepare to %s failed\n",
                CLUSTER_NODES[i]);
      }
    }

    if (ack_count < follower_count) {

      for (int i = 0; i < CLUSTER_NODE_COUNT; i++) {

        if (strcmp(CLUSTER_NODES[i], ctx->node_id) == 0) {
          continue;
        }

        abort_follower(CLUSTER_NODES[i],
                       ctx->port,
                       msg->request_id);
      }

      const char *err = "strong mode prepare failed";

      send_message(client_fd,
                   MSG_ERROR,
                   msg->request_id,
                   err,
                   (uint32_t) strlen(err));

      return 0;
    }

    for (int i = 0; i < CLUSTER_NODE_COUNT; i++) {

      if (strcmp(CLUSTER_NODES[i], ctx->node_id) == 0) {
        continue;
      }

      if (commit_append_to_follower(CLUSTER_NODES[i],
                                    ctx->port,
                                    msg->request_id,
                                    msg->payload,
                                    msg->length) < 0) {
        fprintf(stderr,
                "commit to %s failed\n",
                CLUSTER_NODES[i]);
      }
    }

  } else {

    for (int i = 0; i < CLUSTER_NODE_COUNT; i++) {

      if (strcmp(CLUSTER_NODES[i], ctx->node_id) == 0) {
        continue;
      }

      if (replicate_append_to_follower(CLUSTER_NODES[i],
                                       ctx->port,
                                       msg->request_id,
                                       msg->payload,
                                       msg->length) == 0) {
        ack_count++;
      } else {
        fprintf(stderr,
                "replication to %s failed\n",
                CLUSTER_NODES[i]);
      }
    }

    if (ctx->adaptive_enabled &&
        ctx->mode == CONSISTENCY_EVENTUAL &&
        ack_count == 0) {
      ctx->mode = CONSISTENCY_QUORUM;
      printf("ADAPTIVE: no follower acknowledgments; switching EVENTUAL -> QUORUM\n");
    }

    if (ctx->mode == CONSISTENCY_QUORUM &&
        ack_count < (follower_count / 2)) {

      if (ctx->adaptive_enabled) {
        ctx->mode = CONSISTENCY_STRONG;
        printf("ADAPTIVE: quorum failure; switching QUORUM -> STRONG\n");
      }

      const char *err =
        "quorum mode requires majority replication";

      send_message(client_fd,
                   MSG_ERROR,
                   msg->request_id,
                   err,
                   (uint32_t) strlen(err));

      return 0;
    }
  }

  if (ctx->adaptive_enabled &&
      !conflict_detected &&
      ack_count == follower_count) {
    if (ctx->mode == CONSISTENCY_STRONG) {
      ctx->mode = CONSISTENCY_QUORUM;
      printf("ADAPTIVE: full follower acknowledgments; switching STRONG -> QUORUM\n");
    } else if (ctx->mode == CONSISTENCY_QUORUM) {
      ctx->mode = CONSISTENCY_EVENTUAL;
      printf("ADAPTIVE: full follower acknowledgments; switching QUORUM -> EVENTUAL\n");
    }
  }

  printf("Mode=%d follower_acks=%d/%d\n",
         ctx->mode,
         ack_count,
         follower_count);

  if (ledger_append_local(tx,
                          ctx->current_term,
                          1) < 0) {

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
                       const struct message *msg,
                       server_context_t *ctx) {
  char tx[MAX_TX_SIZE + 1];

  if (msg->length > MAX_TX_SIZE) {
    const char *err = "transaction too large";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  memcpy(tx, msg->payload, msg->length);
  tx[msg->length] = '\0';

  printf("REPL_APPEND tx=\"%s\"\n", tx);

  if (ledger_append_local(tx, ctx->current_term, 1) < 0) {
    const char *err = "ledger full";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  ledger_print();

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_get_log(int client_fd,
                   const struct message *msg) {
  char buf[MAX_PAYLOAD_SIZE];
  int n = ledger_serialize(buf, sizeof(buf));

  if (n < 0) {
    const char *err = "failed to serialize ledger";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (send_message(client_fd, MSG_LOG_RESPONSE, msg->request_id, buf, (uint32_t) n) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_set_mode(int client_fd,
                    const struct message *msg,
                    server_context_t *ctx) {
  char mode[32];

  if (ctx->role != ROLE_LEADER) {
    const char *err = "not leader";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (msg->length == 0 || msg->length >= sizeof(mode)) {
    const char *err = "invalid mode";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  memcpy(mode, msg->payload, msg->length);
  mode[msg->length] = '\0';

  if (strcmp(mode, "strong") == 0) {
    ctx->mode = CONSISTENCY_STRONG;
  } else if (strcmp(mode, "quorum") == 0) {
    ctx->mode = CONSISTENCY_QUORUM;
  } else if (strcmp(mode, "eventual") == 0) {
    ctx->mode = CONSISTENCY_EVENTUAL;
  } else {
    const char *err = "unknown mode";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  printf("Consistency mode set to %s\n", mode);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_prepare_append(int client_fd,
                          const struct message *msg) {
  printf("PREPARE_APPEND request_id=%u\n", msg->request_id);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_commit_append(int client_fd, const struct message *msg, server_context_t *ctx) {
  return handle_repl_append(client_fd, msg, ctx);
}

int handle_abort(int client_fd,
                 const struct message *msg) {
  printf("ABORT request_id=%u\n", msg->request_id);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_sync_request(int client_fd,
                        const struct message *msg) {
  uint8_t *payload;
  uint32_t length;
  char leader_host[256];

  if (msg->length > 0) {
    if (msg->length >= sizeof(leader_host)) {
      const char *err = "leader hostname too long";
      send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
      return 0;
    }

    memcpy(leader_host, msg->payload, msg->length);
    leader_host[msg->length] = '\0';

    if (request_sync_from_leader(leader_host, "5000", msg->request_id) < 0) {
      const char *err = "sync from leader failed";
      send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
      return 0;
    }

    send_message(client_fd, MSG_OK, msg->request_id, NULL, 0);
    return 0;
  }

  payload = NULL;
  length = 0;

  if (ledger_build_sync_payload(&payload, &length) < 0) {
    const char *err = "failed to build sync payload";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (send_message(client_fd, MSG_SYNC_RESPONSE, msg->request_id, payload, length) < 0) {
    perror("send_message");
  }

  free(payload);
  return 0;
}

int handle_sync_response(int client_fd,
                         const struct message *msg) {
  if (ledger_apply_sync_payload(msg->payload, msg->length) < 0) {
    const char *err = "failed to apply sync payload";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  printf("Applied sync payload. New local ledger:\n");
  ledger_print();

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_status(int client_fd,
                  const struct message *msg,
                  server_context_t *ctx) {
  char buf[512];
  int n;
  
  (void) ctx;
  
  n = snprintf(buf,
             sizeof(buf),
             "size=%u last_hash=%s",
             ledger_size(),
             ledger_last_hash());

  if (n < 0 || (uint32_t) n >= sizeof(buf)) {
    const char *err = "failed to build status";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (send_message(client_fd, MSG_STATUS_RESPONSE, msg->request_id, buf, (uint32_t) n) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_repair(int client_fd,
                  const struct message *msg) {
  char leader_host[256];
  char local_status[128];
  char leader_status[128];
  char response[512];
  int repaired;
  int n;

  if (msg->length == 0 || msg->length >= sizeof(leader_host)) {
    const char *err = "invalid leader hostname";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  memcpy(leader_host, msg->payload, msg->length);
  leader_host[msg->length] = '\0';

  n = snprintf(local_status,
               sizeof(local_status),
               "size=%u last_hash=%s",
               ledger_size(),
               ledger_last_hash());

  if (n < 0 || (uint32_t) n >= sizeof(local_status)) {
    const char *err = "failed to build local status";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (request_status_from_node(leader_host, "5000", msg->request_id,
                               leader_status, sizeof(leader_status)) < 0) {
    const char *err = "failed to get leader status";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  repaired = 0;

  if (strcmp(local_status, leader_status) != 0) {
    if (request_sync_from_leader(leader_host, "5000", msg->request_id) < 0) {
      const char *err = "repair sync failed";
      send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
      return 0;
    }

    repaired = 1;
  }

  n = snprintf(response,
               sizeof(response),
               "local_before=\"%s\" leader=\"%s\" repaired=%d",
               local_status,
               leader_status,
               repaired);

  if (n < 0 || (uint32_t) n >= sizeof(response)) {
    const char *err = "failed to build repair response";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (send_message(client_fd, MSG_REPAIR_RESPONSE, msg->request_id,
                   response, (uint32_t) strlen(response)) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_inject_latency(int client_fd,
                          const struct message *msg) {
  char buf[32];
  uint32_t latency_ms;

  if (msg->length == 0 || msg->length >= sizeof(buf)) {
    const char *err = "invalid latency";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(buf, msg->payload, msg->length);
  buf[msg->length] = '\0';

  latency_ms = (uint32_t) strtoul(buf, NULL, 10);

  failure_injector_set_latency(latency_ms);

  if (send_message(client_fd,
                   MSG_INJECT_LATENCY_RESPONSE,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_inject_drop(int client_fd,
                       const struct message *msg) {
  char buf[32];
  uint32_t drop_percent;

  if (msg->length == 0 || msg->length >= sizeof(buf)) {
    const char *err = "invalid drop percentage";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(buf, msg->payload, msg->length);
  buf[msg->length] = '\0';

  drop_percent = (uint32_t) strtoul(buf, NULL, 10);

  failure_injector_set_drop(drop_percent);

  if (send_message(client_fd,
                   MSG_INJECT_DROP_RESPONSE,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_inject_partition(int client_fd,
                            const struct message *msg) {
  char peer[256];

  if (msg->length == 0 || msg->length >= sizeof(peer)) {
    const char *err = "invalid partition peer";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(peer, msg->payload, msg->length);
  peer[msg->length] = '\0';

  failure_injector_set_partition(peer);

  if (send_message(client_fd,
                   MSG_INJECT_PARTITION_RESPONSE,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_inject_heal(int client_fd,
                       const struct message *msg) {
  (void) msg;

  failure_injector_clear_partition();

  if (send_message(client_fd,
                   MSG_INJECT_HEAL_RESPONSE,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_set_adaptive(int client_fd,
                        const struct message *msg,
                        server_context_t *ctx) {
  char mode[16];

  if (ctx->role != ROLE_LEADER) {
    const char *err = "not leader";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (msg->length == 0 || msg->length >= sizeof(mode)) {
    const char *err = "invalid adaptive mode";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  memcpy(mode, msg->payload, msg->length);
  mode[msg->length] = '\0';

  if (strcmp(mode, "on") == 0) {
    ctx->adaptive_enabled = 1;
    ctx->mode = CONSISTENCY_EVENTUAL;
    printf("Adaptive consistency enabled; starting in EVENTUAL mode\n");

  } else if (strcmp(mode, "off") == 0) {
    ctx->adaptive_enabled = 0;
    printf("Adaptive consistency disabled\n");

  } else {
    const char *err = "adaptive must be on or off";

    send_message(client_fd,
                 MSG_ERROR,
                 msg->request_id,
                 err,
                 (uint32_t) strlen(err));

    return 0;
  }

  if (send_message(client_fd,
                   MSG_SET_ADAPTIVE_RESPONSE,
                   msg->request_id,
                   NULL,
                   0) < 0) {
    perror("send_message");
  }

  return 0;
}

static const char *role_to_string(server_role_t role){
  switch (role) {
    case ROLE_LEADER:
      return "LEADER";
    case ROLE_CANDIDATE:
      return "CANDIDATE";
    case ROLE_FOLLOWER:
      return "FOLLOWER";
    default:
      return "UNKNOWN";
  }
}

static const char *mode_to_string(consistency_mode_t mode) {
  switch (mode) {
    case CONSISTENCY_STRONG:
      return "STRONG";
    case CONSISTENCY_QUORUM:
      return "QUORUM";
    case CONSISTENCY_EVENTUAL:
      return "EVENTUAL";
    default:
      return "UNKNOWN";
  }
}

int handle_dashboard(int client_fd,
                     const struct message *msg,
                     server_context_t *ctx) {
  char buf[1024];
  char leader[256];
  int n;

  pthread_mutex_lock(&ctx->lock);

  snprintf(leader,
           sizeof(leader),
           "%s",
           ctx->current_leader[0] != '\0' ? ctx->current_leader : "UNKNOWN");

  n = snprintf(buf,
               sizeof(buf),
               "node=%s role=%s leader=%s term=%u mode=%s adaptive=%d size=%u last_hash=%s",
               ctx->node_id,
               role_to_string(ctx->role),
               leader,
               ctx->current_term,
               mode_to_string(ctx->mode),
               ctx->adaptive_enabled,
               ledger_size(),
               ledger_last_hash());

  pthread_mutex_unlock(&ctx->lock);

  if (n < 0 || (uint32_t) n >= sizeof(buf)) {
    const char *err = "failed to build dashboard status";
    send_message(client_fd, MSG_ERROR, msg->request_id, err, (uint32_t) strlen(err));
    return 0;
  }

  if (send_message(client_fd,
                   MSG_DASHBOARD_RESPONSE,
                   msg->request_id,
                   buf,
                   (uint32_t) n) < 0) {
    perror("send_message");
  }

  return 0;
}