#ifndef LEDGER_H
#define LEDGER_H

#include <stdint.h>
#include <stddef.h>

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
void ledger_clear(void);
int ledger_append_recovered(const char *tx, uint32_t term, uint32_t committed);
int ledger_build_sync_payload(uint8_t **payload_out, uint32_t *length_out);
int ledger_apply_sync_payload(const uint8_t *payload, uint32_t length);
int ledger_load_from_file(const char *path);
int ledger_save_to_file(const char *path);
const char *ledger_last_hash(void);

#endif