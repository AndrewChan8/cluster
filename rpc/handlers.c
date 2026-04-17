#include "handlers.h"
#include "kv_store.h"
#include "replication.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_get(int client_fd, const struct message *msg) {
  char *key;

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed GET key: %s\n", key);

  const char *value = kv_get(key);

  if (value != NULL) {
    uint32_t value_len = (uint32_t) strlen(value);

    if (send_message(client_fd, MSG_GET_RESPONSE,
                     msg->request_id,
                     value,
                     value_len) < 0) {
      perror("send_message");
    }
  } else {
    const char *err = "key not found";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR,
                     msg->request_id,
                     err,
                     err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  return 0;
}

int handle_put(int client_fd, const struct message *msg, server_role_t role, const char *port) {
  if (role != ROLE_LEADER) {
    const char *err = "not leader";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    return 0;
  }

  char *key;
  char *value;

  if (kv_parse_put_payload(msg->payload, msg->length, &key, &value) < 0) {
    perror("kv_parse_put_payload");
    return -1;
  }

  printf("Parsed PUT key: %s value: %s\n", key, value);

  if (kv_put(key, value) == 0) {
    if (replicate_put_to_follower("node2", port, msg->request_id,
                                  msg->payload, msg->length) < 0) {
      const char *err = "replication to node2 failed";
      uint32_t err_len = (uint32_t) strlen(err);

      if (send_message(client_fd, MSG_ERROR,
                       msg->request_id,
                       err,
                       err_len) < 0) {
        perror("send_message");
      }
    } else if (replicate_put_to_follower("node3", port, msg->request_id,
                                         msg->payload, msg->length) < 0) {
      const char *err = "replication to node3 failed";
      uint32_t err_len = (uint32_t) strlen(err);

      if (send_message(client_fd, MSG_ERROR,
                       msg->request_id,
                       err,
                       err_len) < 0) {
        perror("send_message");
      }
    } else {
      if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
        perror("send_message");
      }
    }
  } else {
    const char *err = "store full";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR,
                     msg->request_id,
                     err,
                     err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  free(value);
  return 0;
}

int handle_repl_put(int client_fd, const struct message *msg) {
  char *key;
  char *value;

  if (kv_parse_put_payload(msg->payload, msg->length, &key, &value) < 0) {
    perror("kv_parse_put_payload");
    return -1;
  }

  printf("Parsed REPL_PUT key: %s value: %s\n", key, value);

  if (kv_put(key, value) == 0) {
    if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
      perror("send_message");
    }
  } else {
    const char *err = "store full";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR,
                     msg->request_id,
                     err,
                     err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  free(value);
  return 0;
}

int handle_delete(int client_fd, const struct message *msg, server_role_t role, const char *port) {
  if (role != ROLE_LEADER) {
    const char *err = "not leader";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    return 0;
  }

  char *key;

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed DELETE key: %s\n", key);

  if (kv_delete(key) == 0) {
    if (replicate_delete_to_follower("node2", port, msg->request_id,
                                     msg->payload, msg->length) < 0) {
      const char *err = "replication to node2 failed";
      uint32_t err_len = (uint32_t) strlen(err);

      if (send_message(client_fd, MSG_ERROR,
                       msg->request_id,
                       err,
                       err_len) < 0) {
        perror("send_message");
      }
    } else if (replicate_delete_to_follower("node3", port, msg->request_id,
                                            msg->payload, msg->length) < 0) {
      const char *err = "replication to node3 failed";
      uint32_t err_len = (uint32_t) strlen(err);

      if (send_message(client_fd, MSG_ERROR,
                       msg->request_id,
                       err,
                       err_len) < 0) {
        perror("send_message");
      }
    } else {
      if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
        perror("send_message");
      }
    }
  } else {
    const char *err = "key not found";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR,
                     msg->request_id,
                     err,
                     err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  return 0;
}

int handle_repl_delete(int client_fd, const struct message *msg) {
  char *key;

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed REPL_DELETE key: %s\n", key);

  if (kv_delete(key) == 0) {
    if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
      perror("send_message");
    }
  } else {
    const char *err = "key not found";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR,
                     msg->request_id,
                     err,
                     err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  return 0;
}