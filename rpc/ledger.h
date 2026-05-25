#ifndef LEDGER_H
#define LEDGER_H

#include <stdint.h>

#define MAX_LEDGER_ENTRIES 128
#define MAX_TX_SIZE 256
#define HASH_SIZE 64

typedef struct {
  uint32_t index;
  uint32_t term;
  uint32_t committed;
  char tx[MAX_TX_SIZE];
  char prev_hash[HASH_SIZE + 1];
  char hash[HASH_SIZE + 1];
} ledger_entry_t;

void ledger_init(void);
int ledger_append_local(const char *tx, uint32_t term, uint32_t committed);
const ledger_entry_t *ledger_get(uint32_t index);
uint32_t ledger_size(void);
void ledger_print(void);
int ledger_serialize(char *buf, uint32_t buf_size);

#endif