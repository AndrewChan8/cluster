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
  char *key = NULL;
  char *value = NULL;
  int prepared_node2 = 0;
  int prepared_node3 = 0;

  if (role != ROLE_LEADER) {
    const char *err = "not leader";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    return 0;
  }

  if (kv_parse_put_payload(msg->payload, msg->length, &key, &value) < 0) {
    perror("kv_parse_put_payload");
    return -1;
  }

  printf("Parsed PUT key: %s value: %s\n", key, value);

  if (prepare_put_to_follower("node2", port, msg->request_id,
                              msg->payload, msg->length) < 0) {
    const char *err = "prepare to node2 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  prepared_node2 = 1;

  if (prepare_put_to_follower("node3", port, msg->request_id,
                              msg->payload, msg->length) < 0) {
    const char *err = "prepare to node3 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (prepared_node2) {
      abort_follower("node2", port, msg->request_id);
    }

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  prepared_node3 = 1;

  if (kv_put(key, value) < 0) {
    const char *err = "store full";
    uint32_t err_len = (uint32_t) strlen(err);

    if (prepared_node2) {
      abort_follower("node2", port, msg->request_id);
    }

    if (prepared_node3) {
      abort_follower("node3", port, msg->request_id);
    }

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (commit_put_to_follower("node2", port, msg->request_id,
                             msg->payload, msg->length) < 0) {
    const char *err = "commit to node2 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (commit_put_to_follower("node3", port, msg->request_id,
                             msg->payload, msg->length) < 0) {
    const char *err = "commit to node3 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

cleanup:
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
  char *key = NULL;
  int prepared_node2 = 0;
  int prepared_node3 = 0;

  if (role != ROLE_LEADER) {
    const char *err = "not leader";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    return 0;
  }

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed DELETE key: %s\n", key);

  if (prepare_delete_to_follower("node2", port, msg->request_id,
                                 msg->payload, msg->length) < 0) {
    const char *err = "prepare delete to node2 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  prepared_node2 = 1;

  if (prepare_delete_to_follower("node3", port, msg->request_id,
                                 msg->payload, msg->length) < 0) {
    const char *err = "prepare delete to node3 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (prepared_node2) {
      abort_follower("node2", port, msg->request_id);
    }

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  prepared_node3 = 1;

  if (kv_delete(key) < 0) {
    const char *err = "key not found";
    uint32_t err_len = (uint32_t) strlen(err);

    if (prepared_node2) {
      abort_follower("node2", port, msg->request_id);
    }

    if (prepared_node3) {
      abort_follower("node3", port, msg->request_id);
    }

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (commit_delete_to_follower("node2", port, msg->request_id,
                                msg->payload, msg->length) < 0) {
    const char *err = "commit delete to node2 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (commit_delete_to_follower("node3", port, msg->request_id,
                                msg->payload, msg->length) < 0) {
    const char *err = "commit delete to node3 failed";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }

    goto cleanup;
  }

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

cleanup:
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

int handle_prepare_put(int client_fd, const struct message *msg) {
  char *key;
  char *value;

  if (kv_parse_put_payload(msg->payload, msg->length, &key, &value) < 0) {
    perror("kv_parse_put_payload");
    return -1;
  }

  printf("Parsed PREPARE_PUT key: %s value: %s\n", key, value);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  free(key);
  free(value);
  return 0;
}

int handle_commit_put(int client_fd, const struct message *msg) {
  char *key;
  char *value;

  if (kv_parse_put_payload(msg->payload, msg->length, &key, &value) < 0) {
    perror("kv_parse_put_payload");
    return -1;
  }

  printf("Parsed COMMIT_PUT key: %s value: %s\n", key, value);

  if (kv_put(key, value) == 0) {
    if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
      perror("send_message");
    }
  } else {
    const char *err = "store full";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  free(value);
  return 0;
}

int handle_abort(int client_fd, const struct message *msg) {
  printf("Received ABORT for request_id=%u\n", msg->request_id);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  return 0;
}

int handle_prepare_delete(int client_fd, const struct message *msg) {
  char *key;

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed PREPARE_DELETE key: %s\n", key);

  if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
    perror("send_message");
  }

  free(key);
  return 0;
}

int handle_commit_delete(int client_fd, const struct message *msg) {
  char *key;

  if (kv_parse_get_payload(msg->payload, msg->length, &key) < 0) {
    perror("kv_parse_get_payload");
    return -1;
  }

  printf("Parsed COMMIT_DELETE key: %s\n", key);

  if (kv_delete(key) == 0) {
    if (send_message(client_fd, MSG_OK, msg->request_id, NULL, 0) < 0) {
      perror("send_message");
    }
  } else {
    const char *err = "key not found";
    uint32_t err_len = (uint32_t) strlen(err);

    if (send_message(client_fd, MSG_ERROR, msg->request_id, err, err_len) < 0) {
      perror("send_message");
    }
  }

  free(key);
  return 0;
}

