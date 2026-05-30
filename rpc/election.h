#ifndef ELECTION_H
#define ELECTION_H

#include "server_context.h"

#include "common.h"

int handle_request_vote(int client_fd,
                        const struct message *msg,
                        server_context_t *ctx);

int handle_heartbeat(int client_fd,
                     const struct message *msg,
                     server_context_t *ctx);

void election_init(server_context_t *ctx);

int start_heartbeat_thread(server_context_t *ctx);

int start_election_timeout_thread(server_context_t *ctx);

#endif