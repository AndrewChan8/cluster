#include "anti_entropy.h"
#include "ledger.h"
#include "replication.h"
#include "server_context.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define ANTI_ENTROPY_INTERVAL_SECONDS 5

typedef struct {
  server_context_t *ctx;
} anti_entropy_args_t;

static void *anti_entropy_loop(void *arg) {
  anti_entropy_args_t *args = (anti_entropy_args_t *) arg;
  char local_status[128];
  char leader_status[128];
  char leader_host[256];
  char port[32];
  server_role_t role;

  while (1) {
    sleep(ANTI_ENTROPY_INTERVAL_SECONDS);

    pthread_mutex_lock(&args->ctx->lock);
    role = args->ctx->role;
    snprintf(leader_host, sizeof(leader_host), "%s", args->ctx->current_leader);
    snprintf(port, sizeof(port), "%s", args->ctx->port);
    pthread_mutex_unlock(&args->ctx->lock);

    if (role == ROLE_LEADER || leader_host[0] == '\0') {
      continue;
    }

    int n = snprintf(local_status,
                     sizeof(local_status),
                     "size=%u last_hash=%s",
                     ledger_size(),
                     ledger_last_hash());

    if (n < 0 || (uint32_t) n >= sizeof(local_status)) {
      fprintf(stderr, "anti-entropy: failed to build local status\n");
      continue;
    }

    if (request_status_from_node(leader_host,
                                 port,
                                 1,
                                 leader_status,
                                 sizeof(leader_status)) < 0) {
      fprintf(stderr, "anti-entropy: failed to get leader status from %s\n",
              leader_host);
      continue;
    }

    if (strcmp(local_status, leader_status) != 0) {
      printf("anti-entropy: divergence detected\n");
      printf("anti-entropy: local=%s\n", local_status);
      printf("anti-entropy: leader=%s status=%s\n", leader_host, leader_status);

      if (request_sync_from_leader(leader_host,
                                   port,
                                   1) < 0) {
        fprintf(stderr, "anti-entropy: repair failed\n");
      } else {
        printf("anti-entropy: repair completed from %s\n", leader_host);
      }
    }
  }

  return NULL;
}

void start_anti_entropy_thread(server_context_t *ctx) {
  pthread_t thread;
  anti_entropy_args_t *args;

  args = malloc(sizeof(*args));
  if (args == NULL) {
    perror("malloc");
    return;
  }

  args->ctx = ctx;

  if (pthread_create(&thread, NULL, anti_entropy_loop, args) != 0) {
    perror("pthread_create");
    free(args);
    return;
  }

  pthread_detach(thread);
}