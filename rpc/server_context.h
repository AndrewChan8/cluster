#ifndef SERVER_CONTEXT_H
#define SERVER_CONTEXT_H

#include <pthread.h>
#include <stdint.h>

typedef enum {
  ROLE_FOLLOWER,
  ROLE_CANDIDATE,
  ROLE_LEADER
} server_role_t;

typedef enum {
  CONSISTENCY_STRONG,
  CONSISTENCY_QUORUM,
  CONSISTENCY_EVENTUAL
} consistency_mode_t;

typedef struct {
  char node_id[256];
  char port[32];

  server_role_t role;
  uint32_t current_term;
  char voted_for[256];
  char current_leader[256];
  uint64_t last_heartbeat_ms;

  consistency_mode_t mode;

  pthread_mutex_t lock;
} server_context_t;

#endif