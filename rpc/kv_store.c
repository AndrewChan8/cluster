#include "kv_store.h"

#include <string.h>
#include <stddef.h>

#define MAX_KV_PAIRS 16
#define MAX_KEY_SIZE 256
#define MAX_VALUE_SIZE 256

struct kv_pair {
  char key[MAX_KEY_SIZE];
  char value[MAX_VALUE_SIZE];
};

static struct kv_pair store[MAX_KV_PAIRS];
static size_t store_count = 0;

void kv_store_init(void) {
  strcpy(store[0].key, "apple");
  strcpy(store[0].value, "red");
  store_count = 1;
}

const char *kv_get(const char *key) {
  for (size_t i = 0; i < store_count; i++) {
    if (strcmp(store[i].key, key) == 0) {
      return store[i].value;
    }
  }
  return NULL;
}

int kv_put(const char *key, const char *value) {
  for (size_t i = 0; i < store_count; i++) {
    if (strcmp(store[i].key, key) == 0) {
      strncpy(store[i].value, value, MAX_VALUE_SIZE - 1);
      store[i].value[MAX_VALUE_SIZE - 1] = '\0';
      return 0;
    }
  }

  if (store_count >= MAX_KV_PAIRS) {
    return -1;
  }

  strncpy(store[store_count].key, key, MAX_KEY_SIZE - 1);
  store[store_count].key[MAX_KEY_SIZE - 1] = '\0';

  strncpy(store[store_count].value, value, MAX_VALUE_SIZE - 1);
  store[store_count].value[MAX_VALUE_SIZE - 1] = '\0';

  store_count++;
  return 0;
}

int kv_delete(const char *key) {
  for (size_t i = 0; i < store_count; i++) {
    if (strcmp(store[i].key, key) == 0) {
      for (size_t j = i; j + 1 < store_count; j++) {
        store[j] = store[j + 1];
      }
      store_count--;
      return 0;
    }
  }

  return -1;
}