#include "ledger.h"

#include <stdio.h>
#include <string.h>

static ledger_entry_t ledger[MAX_LEDGER_ENTRIES];
static uint32_t ledger_count = 0;

static void compute_fake_hash(const ledger_entry_t *entry, char out[HASH_SIZE + 1]) {
  unsigned long h = 5381;
  const unsigned char *p;

  h = ((h << 5) + h) + entry->index;
  h = ((h << 5) + h) + entry->term;
  h = ((h << 5) + h) + entry->committed;

  for (p = (const unsigned char *) entry->tx; *p != '\0'; p++) {
    h = ((h << 5) + h) + *p;
  }

  for (p = (const unsigned char *) entry->prev_hash; *p != '\0'; p++) {
    h = ((h << 5) + h) + *p;
  }

  snprintf(out, HASH_SIZE + 1, "%064lx", h);
}

void ledger_init(void) {
  ledger_count = 0;
}

int ledger_append_local(const char *tx, uint32_t term, uint32_t committed) {
  ledger_entry_t *entry;

  if (tx == NULL) {
    return -1;
  }

  if (ledger_count >= MAX_LEDGER_ENTRIES) {
    return -1;
  }

  entry = &ledger[ledger_count];

  entry->index = ledger_count;
  entry->term = term;
  entry->committed = committed;

  strncpy(entry->tx, tx, MAX_TX_SIZE - 1);
  entry->tx[MAX_TX_SIZE - 1] = '\0';

  if (ledger_count == 0) {
    strncpy(entry->prev_hash, "GENESIS", HASH_SIZE);
  } else {
    strncpy(entry->prev_hash, ledger[ledger_count - 1].hash, HASH_SIZE);
  }
  entry->prev_hash[HASH_SIZE] = '\0';

  compute_fake_hash(entry, entry->hash);

  ledger_count++;
  return 0;
}

const ledger_entry_t *ledger_get(uint32_t index) {
  if (index >= ledger_count) {
    return NULL;
  }

  return &ledger[index];
}

uint32_t ledger_size(void) {
  return ledger_count;
}

void ledger_print(void) {
  uint32_t i;

  printf("Ledger size: %u\n", ledger_count);

  for (i = 0; i < ledger_count; i++) {
    printf("[%u] term=%u committed=%u tx=\"%s\"\n",
           ledger[i].index,
           ledger[i].term,
           ledger[i].committed,
           ledger[i].tx);

    printf("     prev_hash=%s\n", ledger[i].prev_hash);
    printf("     hash=%s\n", ledger[i].hash);
  }
}