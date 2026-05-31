/*
  common.h

  Shared networking and protocol definitions for the replicated ledger system.
  Defines message types, framed message structure, socket helpers, and reliable
  read/write utilities used by both clients and servers.
*/


#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define BACKLOG 16
#define MAX_PAYLOAD_SIZE 65536

enum msg_type {
  MSG_PING = 1,
  MSG_PONG = 2,
  MSG_ECHO = 3,

  MSG_APPEND = 4,
  MSG_GET_LOG = 5,
  MSG_SET_MODE = 6,

  MSG_OK = 7,
  MSG_LOG_RESPONSE = 8,
  MSG_ERROR = 9,

  MSG_REPL_APPEND = 10,
  MSG_PREPARE_APPEND = 11,
  MSG_COMMIT_APPEND = 12,
  MSG_ABORT = 13,
  MSG_SYNC_REQUEST = 14,
  MSG_SYNC_RESPONSE = 15,
  MSG_STATUS = 16,
  MSG_STATUS_RESPONSE = 17,
  MSG_REPAIR = 18,
  MSG_REPAIR_RESPONSE = 19,
  
  MSG_REQUEST_VOTE = 20,
  MSG_REQUEST_VOTE_RESPONSE = 21,
  MSG_HEARTBEAT = 22,
  MSG_HEARTBEAT_RESPONSE = 23,

  MSG_INJECT_LATENCY = 24,
  MSG_INJECT_LATENCY_RESPONSE = 25,
  MSG_INJECT_DROP = 26,
  MSG_INJECT_DROP_RESPONSE = 27,
  MSG_INJECT_PARTITION = 28,
  MSG_INJECT_PARTITION_RESPONSE = 29,
  MSG_INJECT_HEAL = 30,
  MSG_INJECT_HEAL_RESPONSE = 31,

  MSG_SET_ADAPTIVE = 32,
  MSG_SET_ADAPTIVE_RESPONSE = 33,

  MSG_DASHBOARD = 34,
  MSG_DASHBOARD_RESPONSE = 35
};

struct message {
  uint32_t type;
  uint32_t request_id;
  uint32_t length;
  uint8_t *payload;
};

void die(const char *msg);

int create_server_socket(const char *port);
int connect_to_server(const char *host, const char *port);

ssize_t read_n(int fd, void *buf, size_t n);
ssize_t write_n(int fd, const void *buf, size_t n);

int send_message(int fd, uint32_t type, uint32_t request_id,
                 const void *payload, uint32_t length);
int recv_message(int fd, struct message *msg);
void free_message(struct message *msg);

#endif