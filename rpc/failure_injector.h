#ifndef FAILURE_INJECTOR_H
#define FAILURE_INJECTOR_H

#include <stdint.h>

void failure_injector_init(void);

void failure_injector_set_latency(uint32_t latency_ms);

uint32_t failure_injector_get_latency(void);

void failure_inject_before_send(const char *peer,
                                uint32_t msg_type);

#endif