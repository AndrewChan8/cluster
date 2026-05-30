/*
  handlers.h

  Defines protocol message handlers for distributed ledger operations.
  Handlers process client requests, replication events, synchronization,
  consistency mode coordination, and replica status operations.
*/

#ifndef HANDLERS_H
#define HANDLERS_H

#include "common.h"
#include "server_context.h"

int handle_append(int client_fd, const struct message *msg, server_context_t *ctx);

int handle_repl_append(int client_fd, const struct message *msg, server_context_t *ctx);

int handle_get_log(int client_fd, const struct message *msg);

int handle_prepare_append(int client_fd, const struct message *msg);

int handle_commit_append(int client_fd, const struct message *msg, server_context_t *ctx);

int handle_abort(int client_fd, const struct message *msg);

int handle_set_mode(int client_fd, const struct message *msg, server_context_t *ctx);

int handle_sync_request(int client_fd, const struct message *msg);

int handle_sync_response(int client_fd, const struct message *msg);

int handle_status(int client_fd, const struct message *msg, server_context_t *ctx);

int handle_repair(int client_fd, const struct message *msg);

#endif