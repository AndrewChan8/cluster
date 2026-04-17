#ifndef KV_STORE_H
#define KV_STORE_H

const char *kv_get(const char *key);
int kv_put(const char *key, const char *value);
int kv_delete(const char *key);
void kv_store_init(void);

#endif