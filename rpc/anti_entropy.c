#include "anti_entropy.h"
#include "ledger.h"
#include "replication.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define ANTI_ENTROPY_INTERVAL_SECONDS 5

typedef struct {
  char leader_host[256];
  char port[32];
} anti_entropy_args_t;

static void *anti_entropy_loop(void *arg) {
  anti_entropy_args_t *args = (anti_entropy_args_t *) arg;
  char local_status[128];
  char leader_status[128];

  while (1) {
    sleep(ANTI_ENTROPY_INTERVAL_SECONDS);

    int n;

    n = snprintf(local_status,
                sizeof(local_status),
                "size=%u last_hash=%s",
                ledger_size(),
                ledger_last_hash());

    if (n < 0 || (uint32_t) n >= sizeof(local_status)) {
      fprintf(stderr, "anti-entropy: failed to build local status\n");
      continue;
    }

    if (request_status_from_node(args->leader_host,
                                 args->port,
                                 1,
                                 leader_status,
                                 sizeof(leader_status)) < 0) {
      fprintf(stderr, "anti-entropy: failed to get leader status\n");
      continue;
    }

    if (strcmp(local_status, leader_status) != 0) {
      printf("anti-entropy: divergence detected\n");
      printf("anti-entropy: local=%s\n", local_status);
      printf("anti-entropy: leader=%s\n", leader_status);

      if (request_sync_from_leader(args->leader_host,
                                   args->port,
                                   1) < 0) {
        fprintf(stderr, "anti-entropy: repair failed\n");
      } else {
        printf("anti-entropy: repair completed\n");
      }
    }
  }

  return NULL;
}

void start_anti_entropy_thread(const char *leader_host,
                               const char *port) {
  pthread_t thread;
  anti_entropy_args_t *args;

  args = malloc(sizeof(*args));
  if (args == NULL) {
    perror("malloc");
    return;
  }

  strncpy(args->leader_host, leader_host, sizeof(args->leader_host) - 1);
  args->leader_host[sizeof(args->leader_host) - 1] = '\0';

  strncpy(args->port, port, sizeof(args->port) - 1);
  args->port[sizeof(args->port) - 1] = '\0';

  if (pthread_create(&thread, NULL, anti_entropy_loop, args) != 0) {
    perror("pthread_create");
    free(args);
    return;
  }

  pthread_detach(thread);
}