#include "common.h"
#include "kv_store.h"
#include "replication.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef enum {
  ROLE_LEADER,
  ROLE_FOLLOWER
} server_role_t;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    perror("gethostname");
    return EXIT_FAILURE;
  }
  hostname[sizeof(hostname) - 1] = '\0';

  server_role_t role;
  if (strcmp(hostname, "node1") == 0) {
    role = ROLE_LEADER;
  } else {
    role = ROLE_FOLLOWER;
  }

  int listen_fd = create_server_socket(argv[1]);
  printf("Server listening on port %s as %s (%s)\n",
         argv[1],
         role == ROLE_LEADER ? "LEADER" : "FOLLOWER",
         hostname);

  kv_store_init();

  while (1) {
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &addr_len);
    if (client_fd == -1) {
      perror("accept");
      continue;
    }

    struct message msg;
    if (recv_message(client_fd, &msg) < 0) {
      perror("recv_message");
      close(client_fd);
      continue;
    }

    printf("Received message: type=%u request_id=%u length=%u\n",
           msg.type, msg.request_id, msg.length);

    if (msg.type == MSG_PING) {
      if (send_message(client_fd, MSG_PONG, msg.request_id, NULL, 0) < 0) {
        perror("send_message");
      }

    } else if (msg.type == MSG_ECHO) {
      if (send_message(client_fd, MSG_ECHO, msg.request_id, msg.payload, msg.length) < 0) {
        perror("send_message");
      }

    } else if (msg.type == MSG_GET) {
      char *key;

      if (kv_parse_get_payload(msg.payload, msg.length, &key) < 0) {
        perror("kv_parse_get_payload");
      } else {
        printf("Parsed GET key: %s\n", key);

        const char *value = kv_get(key);

        if (value != NULL) {
          uint32_t value_len = (uint32_t) strlen(value);

          if (send_message(client_fd, MSG_GET_RESPONSE,
                           msg.request_id,
                           value,
                           value_len) < 0) {
            perror("send_message");
          }
        } else {
          const char *err = "key not found";
          uint32_t err_len = (uint32_t) strlen(err);

          if (send_message(client_fd, MSG_ERROR,
                           msg.request_id,
                           err,
                           err_len) < 0) {
            perror("send_message");
          }
        }

        free(key);
      }

    } else if (msg.type == MSG_PUT) {
      if (role != ROLE_LEADER) {
        const char *err = "not leader";
        uint32_t err_len = (uint32_t) strlen(err);

        if (send_message(client_fd, MSG_ERROR, msg.request_id, err, err_len) < 0) {
          perror("send_message");
        }

        free_message(&msg);
        close(client_fd);
        continue;
      }

      char *key;
      char *value;

      if (kv_parse_put_payload(msg.payload, msg.length, &key, &value) < 0) {
        perror("kv_parse_put_payload");
      } else {
        printf("Parsed PUT key: %s value: %s\n", key, value);

        if (kv_put(key, value) == 0) {
          if (replicate_put_to_follower("node2", argv[1], msg.request_id,
                                        msg.payload, msg.length) < 0) {
            const char *err = "replication to node2 failed";
            uint32_t err_len = (uint32_t) strlen(err);

            if (send_message(client_fd, MSG_ERROR,
                             msg.request_id,
                             err,
                             err_len) < 0) {
              perror("send_message");
            }
          } else if (replicate_put_to_follower("node3", argv[1], msg.request_id,
                                               msg.payload, msg.length) < 0) {
            const char *err = "replication to node3 failed";
            uint32_t err_len = (uint32_t) strlen(err);

            if (send_message(client_fd, MSG_ERROR,
                             msg.request_id,
                             err,
                             err_len) < 0) {
              perror("send_message");
            }
          } else {
            if (send_message(client_fd, MSG_OK, msg.request_id, NULL, 0) < 0) {
              perror("send_message");
            }
          }
        } else {
          const char *err = "store full";
          uint32_t err_len = (uint32_t) strlen(err);

          if (send_message(client_fd, MSG_ERROR,
                           msg.request_id,
                           err,
                           err_len) < 0) {
            perror("send_message");
          }
        }

        free(key);
        free(value);
      }

    } else if (msg.type == MSG_REPL_PUT) {
      char *key;
      char *value;

      if (kv_parse_put_payload(msg.payload, msg.length, &key, &value) < 0) {
        perror("kv_parse_put_payload");
      } else {
        printf("Parsed REPL_PUT key: %s value: %s\n", key, value);

        if (kv_put(key, value) == 0) {
          if (send_message(client_fd, MSG_OK, msg.request_id, NULL, 0) < 0) {
            perror("send_message");
          }
        } else {
          const char *err = "store full";
          uint32_t err_len = (uint32_t) strlen(err);

          if (send_message(client_fd, MSG_ERROR,
                           msg.request_id,
                           err,
                           err_len) < 0) {
            perror("send_message");
          }
        }

        free(key);
        free(value);
      }

    } else if (msg.type == MSG_DELETE) {
      if (role != ROLE_LEADER) {
        const char *err = "not leader";
        uint32_t err_len = (uint32_t) strlen(err);

        if (send_message(client_fd, MSG_ERROR, msg.request_id, err, err_len) < 0) {
          perror("send_message");
        }

        free_message(&msg);
        close(client_fd);
        continue;
      }

      char *key;

      if (kv_parse_get_payload(msg.payload, msg.length, &key) < 0) {
        perror("kv_parse_get_payload");
      } else {
        printf("Parsed DELETE key: %s\n", key);

        if (kv_delete(key) == 0) {
          if (send_message(client_fd, MSG_OK, msg.request_id, NULL, 0) < 0) {
            perror("send_message");
          }
        } else {
          const char *err = "key not found";
          uint32_t err_len = (uint32_t) strlen(err);

          if (send_message(client_fd, MSG_ERROR,
                           msg.request_id,
                           err,
                           err_len) < 0) {
            perror("send_message");
          }
        }

        free(key);
      }

    } else {
      fprintf(stderr, "Unknown message type: %u\n", msg.type);
    }

    free_message(&msg);
    close(client_fd);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}