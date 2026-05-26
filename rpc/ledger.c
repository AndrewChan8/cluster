#include "ledger.h"

#include <stdlib.h>
#include <arpa/inet.h>
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

int ledger_serialize(char *buf, uint32_t buf_size) {
  uint32_t i;
  int written;
  uint32_t used = 0;

  if (buf == NULL || buf_size == 0) {
    return -1;
  }

  written = snprintf(buf + used, buf_size - used,
                     "Ledger size: %u\n", ledger_count);
  if (written < 0 || (uint32_t) written >= buf_size - used) {
    return -1;
  }
  used += (uint32_t) written;

  for (i = 0; i < ledger_count; i++) {
    written = snprintf(buf + used, buf_size - used,
                       "[%u] term=%u committed=%u tx=\"%s\"\n"
                       "     prev_hash=%s\n"
                       "     hash=%s\n",
                       ledger[i].index,
                       ledger[i].term,
                       ledger[i].committed,
                       ledger[i].tx,
                       ledger[i].prev_hash,
                       ledger[i].hash);

    if (written < 0 || (uint32_t) written >= buf_size - used) {
      return -1;
    }

    used += (uint32_t) written;
  }

  return (int) used;
}

void ledger_clear(void) {
  ledger_count = 0;
}

int ledger_append_recovered(const char *tx,
                            uint32_t term,
                            uint32_t committed) {
  return ledger_append_local(tx, term, committed);
}

int ledger_build_sync_payload(uint8_t **payload_out,
                              uint32_t *length_out) {
  uint32_t i;
  uint32_t total_length;
  uint32_t offset;
  uint32_t net_count;
  uint8_t *payload;

  if (payload_out == NULL || length_out == NULL) {
    return -1;
  }

  total_length = sizeof(uint32_t);

  for (i = 0; i < ledger_count; i++) {
    total_length += sizeof(uint32_t) * 3;
    total_length += (uint32_t) strlen(ledger[i].tx);
  }

  payload = malloc(total_length);
  if (payload == NULL) {
    return -1;
  }

  offset = 0;

  net_count = htonl(ledger_count);
  memcpy(payload + offset, &net_count, sizeof(net_count));
  offset += sizeof(net_count);

  for (i = 0; i < ledger_count; i++) {
    uint32_t tx_length = (uint32_t) strlen(ledger[i].tx);
    uint32_t net_term = htonl(ledger[i].term);
    uint32_t net_committed = htonl(ledger[i].committed);
    uint32_t net_tx_length = htonl(tx_length);

    memcpy(payload + offset, &net_term, sizeof(net_term));
    offset += sizeof(net_term);

    memcpy(payload + offset, &net_committed, sizeof(net_committed));
    offset += sizeof(net_committed);

    memcpy(payload + offset, &net_tx_length, sizeof(net_tx_length));
    offset += sizeof(net_tx_length);

    memcpy(payload + offset, ledger[i].tx, tx_length);
    offset += tx_length;
  }

  *payload_out = payload;
  *length_out = total_length;
  return 0;
}

int ledger_apply_sync_payload(const uint8_t *payload,
                              uint32_t length) {
  uint32_t offset;
  uint32_t net_count;
  uint32_t count;
  uint32_t i;

  if (payload == NULL || length < sizeof(uint32_t)) {
    return -1;
  }

  offset = 0;

  memcpy(&net_count, payload + offset, sizeof(net_count));
  count = ntohl(net_count);
  offset += sizeof(net_count);

  if (count > MAX_LEDGER_ENTRIES) {
    return -1;
  }

  ledger_clear();

  for (i = 0; i < count; i++) {
    uint32_t net_term;
    uint32_t net_committed;
    uint32_t net_tx_length;
    uint32_t term;
    uint32_t committed;
    uint32_t tx_length;
    char tx[MAX_TX_SIZE + 1];

    if (length - offset < sizeof(uint32_t) * 3) {
      ledger_clear();
      return -1;
    }

    memcpy(&net_term, payload + offset, sizeof(net_term));
    offset += sizeof(net_term);

    memcpy(&net_committed, payload + offset, sizeof(net_committed));
    offset += sizeof(net_committed);

    memcpy(&net_tx_length, payload + offset, sizeof(net_tx_length));
    offset += sizeof(net_tx_length);

    term = ntohl(net_term);
    committed = ntohl(net_committed);
    tx_length = ntohl(net_tx_length);

    if (tx_length >= MAX_TX_SIZE || length - offset < tx_length) {
      ledger_clear();
      return -1;
    }

    memcpy(tx, payload + offset, tx_length);
    tx[tx_length] = '\0';
    offset += tx_length;

    if (ledger_append_recovered(tx, term, committed) < 0) {
      ledger_clear();
      return -1;
    }
  }

  if (offset != length) {
    ledger_clear();
    return -1;
  }

  return 0;
}