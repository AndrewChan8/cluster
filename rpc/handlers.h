#ifndef HANDLERS_H
#define HANDLERS_H

#include "common.h"
#include "server_context.h"

int handle_get(int client_fd, const struct message *msg);
int handle_put(int client_fd, const struct message *msg, server_role_t role, const char *port);
int handle_repl_put(int client_fd, const struct message *msg);
int handle_delete(int client_fd, const struct message *msg, server_role_t role, const char *port);
int handle_repl_delete(int client_fd, const struct message *msg);
int handle_prepare_put(int client_fd, const struct message *msg);
int handle_commit_put(int client_fd, const struct message *msg);
int handle_abort(int client_fd, const struct message *msg);
int handle_prepare_delete(int client_fd, const struct message *msg);
int handle_commit_delete(int client_fd, const struct message *msg);

#endif