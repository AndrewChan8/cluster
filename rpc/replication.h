#ifndef REPLICATION_H
#define REPLICATION_H

#include <stdint.h>

int replicate_put_to_follower(const char *host,
                              const char *port,
                              uint32_t request_id,
                              const uint8_t *payload,
                              uint32_t payload_length);

int replicate_delete_to_follower(const char *host,
                                 const char *port,
                                 uint32_t request_id,
                                 const uint8_t *payload,
                                 uint32_t payload_length);

int prepare_put_to_follower(const char *host,
                            const char *port,
                            uint32_t request_id,
                            const uint8_t *payload,
                            uint32_t payload_length);

int commit_put_to_follower(const char *host,
                           const char *port,
                           uint32_t request_id,
                           const uint8_t *payload,
                           uint32_t payload_length);

int abort_follower(const char *host,
                   const char *port,
                   uint32_t request_id);

int prepare_delete_to_follower(const char *host,
                               const char *port,
                               uint32_t request_id,
                               const uint8_t *payload,
                               uint32_t payload_length);

int commit_delete_to_follower(const char *host,
                              const char *port,
                              uint32_t request_id,
                              const uint8_t *payload,
                              uint32_t payload_length);

#endif