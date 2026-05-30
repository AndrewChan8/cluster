#include "election.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

static const char *PEERS[] = {
  "node1",
  "node2",
  "node3"
};

static const int PEER_COUNT = 3;

static uint64_t now_ms(void) {
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }

  return ((uint64_t) tv.tv_sec * 1000) +
         ((uint64_t) tv.tv_usec / 1000);
}

static void send_heartbeat_to_peer(const char *peer,
                                   const char *port,
                                   uint32_t term,
                                   const char *leader_id) {
  int fd;
  struct message resp;
  char payload[512];

  memset(&resp, 0, sizeof(resp));

  snprintf(payload, sizeof(payload), "%u %s", term, leader_id);

  fd = connect_to_server(peer, port);
  if (fd < 0) {
    return;
  }

  if (send_message(fd, MSG_HEARTBEAT, 1, payload,
                   (uint32_t) strlen(payload)) < 0) {
    close(fd);
    return;
  }

  if (recv_message(fd, &resp) == 0) {
    free_message(&resp);
  }

  close(fd);
}

static int request_vote_from_peer(const char *peer,
                                  const char *port,
                                  uint32_t term,
                                  const char *candidate_id) {
  int fd;
  struct message resp;
  int granted = 0;
  char payload[512];

  memset(&resp, 0, sizeof(resp));

  snprintf(payload, sizeof(payload), "%u %s", term, candidate_id);

  fd = connect_to_server(peer, port);
  if (fd < 0) {
    return 0;
  }

  if (send_message(fd, MSG_REQUEST_VOTE, 1, payload,
                   (uint32_t) strlen(payload)) < 0) {
    close(fd);
    return 0;
  }

  if (recv_message(fd, &resp) == 0) {
    if (resp.type == MSG_REQUEST_VOTE_RESPONSE &&
        resp.length == strlen("granted") &&
        memcmp(resp.payload, "granted", resp.length) == 0) {
      granted = 1;
    }

    free_message(&resp);
  }

  close(fd);
  return granted;
}

static void *heartbeat_thread_main(void *arg) {
  server_context_t *ctx = arg;

  while (1) {
    char self[256];
    char port[32];
    int is_leader;
    uint32_t term;

    pthread_mutex_lock(&ctx->lock);
    is_leader = (ctx->role == ROLE_LEADER);
    term = ctx->current_term;
    snprintf(self, sizeof(self), "%s", ctx->node_id);
    snprintf(port, sizeof(port), "%s", ctx->port);
    pthread_mutex_unlock(&ctx->lock);

    if (is_leader) {
      for (int i = 0; i < PEER_COUNT; i++) {
        if (strcmp(PEERS[i], self) != 0) {
          send_heartbeat_to_peer(PEERS[i], port, term, self);
        }
      }
    }

    sleep(1);
  }

  return NULL;
}

static void *election_timeout_thread_main(void *arg) {
  server_context_t *ctx = arg;
  int timeout_ms = 6000 + (rand() % 3000);

  while (1) {
    uint64_t now;
    uint64_t elapsed;
    int should_become_candidate;

    sleep(1);

    pthread_mutex_lock(&ctx->lock);

    now = now_ms();
    elapsed = now - ctx->last_heartbeat_ms;

    should_become_candidate =
      (ctx->role == ROLE_FOLLOWER &&
       elapsed > (uint64_t) timeout_ms);

    if (should_become_candidate) {
      char candidate_id[256];
      char port[32];
      uint32_t term;
      int votes = 1;

      ctx->role = ROLE_CANDIDATE;
      ctx->current_term++;
      term = ctx->current_term;

      snprintf(ctx->voted_for,
               sizeof(ctx->voted_for),
               "%s",
               ctx->node_id);

      snprintf(candidate_id,
               sizeof(candidate_id),
               "%s",
               ctx->node_id);

      snprintf(port,
               sizeof(port),
               "%s",
               ctx->port);

      printf("Election timeout: node=%s becoming CANDIDATE term=%u\n",
             ctx->node_id,
             ctx->current_term);

      pthread_mutex_unlock(&ctx->lock);

      for (int i = 0; i < PEER_COUNT; i++) {
        if (strcmp(PEERS[i], candidate_id) != 0) {
          votes += request_vote_from_peer(
                     PEERS[i],
                     port,
                     term,
                     candidate_id);
        }
      }

      pthread_mutex_lock(&ctx->lock);

      if (ctx->role == ROLE_CANDIDATE &&
          ctx->current_term == term &&
          votes >= 2) {

        ctx->role = ROLE_LEADER;

        snprintf(ctx->current_leader,
                 sizeof(ctx->current_leader),
                 "%s",
                 ctx->node_id);

        printf("Leader elected: node=%s term=%u votes=%d\n",
               ctx->node_id,
               ctx->current_term,
               votes);
      }

      timeout_ms = 6000 + (rand() % 3000);
    }

    pthread_mutex_unlock(&ctx->lock);
  }

  return NULL;
}

void election_init(server_context_t *ctx) {
  srand((unsigned int) now_ms() ^ (unsigned int) getpid());

  pthread_mutex_lock(&ctx->lock);
  ctx->last_heartbeat_ms = now_ms();
  pthread_mutex_unlock(&ctx->lock);
}

int handle_request_vote(int client_fd,
                        const struct message *msg,
                        server_context_t *ctx) {
  char payload[512];
  char candidate_id[256];
  uint32_t incoming_term;
  const char *response;

  if (msg->length == 0 || msg->length >= sizeof(payload)) {
    return send_message(client_fd, MSG_ERROR, msg->request_id,
                        "invalid vote request",
                        strlen("invalid vote request"));
  }

  memcpy(payload, msg->payload, msg->length);
  payload[msg->length] = '\0';

  if (sscanf(payload, "%u %255s", &incoming_term, candidate_id) != 2) {
    return send_message(client_fd, MSG_ERROR, msg->request_id,
                        "invalid vote request",
                        strlen("invalid vote request"));
  }

  pthread_mutex_lock(&ctx->lock);

  if (incoming_term < ctx->current_term) {
    response = "rejected";

  } else {
    if (incoming_term > ctx->current_term) {
      ctx->current_term = incoming_term;
      ctx->role = ROLE_FOLLOWER;
      ctx->voted_for[0] = '\0';
    }

    if (ctx->voted_for[0] == '\0' ||
        strcmp(ctx->voted_for, candidate_id) == 0) {
      snprintf(ctx->voted_for,
               sizeof(ctx->voted_for),
               "%s",
               candidate_id);

      ctx->last_heartbeat_ms = now_ms();
      ctx->role = ROLE_FOLLOWER;
      response = "granted";
    } else {
      response = "rejected";
    }
  }

  pthread_mutex_unlock(&ctx->lock);

  printf("REQUEST_VOTE term=%u candidate=%s result=%s\n",
         incoming_term,
         candidate_id,
         response);

  return send_message(client_fd, MSG_REQUEST_VOTE_RESPONSE, msg->request_id,
                      response, (uint32_t) strlen(response));
}

int handle_heartbeat(int client_fd,
                     const struct message *msg,
                     server_context_t *ctx) {
  char payload[512];
  char leader_id[256];
  uint32_t incoming_term;

  if (msg->length == 0 || msg->length >= sizeof(payload)) {
    return send_message(client_fd, MSG_ERROR, msg->request_id,
                        "invalid heartbeat", strlen("invalid heartbeat"));
  }

  memcpy(payload, msg->payload, msg->length);
  payload[msg->length] = '\0';

  if (sscanf(payload, "%u %255s", &incoming_term, leader_id) != 2) {
    return send_message(client_fd, MSG_ERROR, msg->request_id,
                        "invalid heartbeat", strlen("invalid heartbeat"));
  }

  pthread_mutex_lock(&ctx->lock);

  if (incoming_term >= ctx->current_term) {
    ctx->current_term = incoming_term;
    ctx->last_heartbeat_ms = now_ms();
    ctx->role = ROLE_FOLLOWER;

    snprintf(ctx->current_leader,
             sizeof(ctx->current_leader),
             "%s",
             leader_id);
  }

  pthread_mutex_unlock(&ctx->lock);

  return send_message(client_fd, MSG_HEARTBEAT_RESPONSE, msg->request_id,
                      NULL, 0);
}

int start_heartbeat_thread(server_context_t *ctx) {
  pthread_t tid;

  if (pthread_create(&tid, NULL, heartbeat_thread_main, ctx) != 0) {
    return -1;
  }

  pthread_detach(tid);
  return 0;
}

int start_election_timeout_thread(server_context_t *ctx) {
  pthread_t tid;

  if (pthread_create(&tid, NULL, election_timeout_thread_main, ctx) != 0) {
    return -1;
  }

  pthread_detach(tid);
  return 0;
}