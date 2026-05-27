/*
  replication.h

  Defines inter-node replication and synchronization operations used by the
  distributed ledger system. Provides helper functions for append replication,
  strong consistency prepare/commit flows, abort handling, and recovery sync.
*/

#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdint.h>

int replicate_append_to_follower(const char *host,
                                 const char *port,
                                 uint32_t request_id,
                                 const uint8_t *payload,
                                 uint32_t payload_length);

int prepare_append_to_follower(const char *host,
                               const char *port,
                               uint32_t request_id,
                               const uint8_t *payload,
                               uint32_t payload_length);

int commit_append_to_follower(const char *host,
                              const char *port,
                              uint32_t request_id,
                              const uint8_t *payload,
                              uint32_t payload_length);

int abort_follower(const char *host,
                   const char *port,
                   uint32_t request_id);

int request_sync_from_leader(const char *leader_host,
                             const char *port,
                             uint32_t request_id);

int request_status_from_node(const char *host,
                             const char *port,
                             uint32_t request_id,
                             char *status_buf,
                             uint32_t status_buf_size);

#endif