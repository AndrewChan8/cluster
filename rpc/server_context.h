#ifndef SERVER_CONTEXT_H
#define SERVER_CONTEXT_H

typedef enum {
  ROLE_LEADER,
  ROLE_FOLLOWER
} server_role_t;

typedef enum {
  CONSISTENCY_STRONG,
  CONSISTENCY_QUORUM,
  CONSISTENCY_EVENTUAL
} consistency_mode_t;

#endif