#include "failure_injector.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t injector_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t injected_latency_ms = 0;

void failure_injector_init(void) {
  pthread_mutex_lock(&injector_lock);
  injected_latency_ms = 0;
  pthread_mutex_unlock(&injector_lock);
}

void failure_injector_set_latency(uint32_t latency_ms) {
  pthread_mutex_lock(&injector_lock);
  injected_latency_ms = latency_ms;
  pthread_mutex_unlock(&injector_lock);

  printf("failure-injector: latency set to %u ms\n", latency_ms);
}

uint32_t failure_injector_get_latency(void) {
  uint32_t latency_ms;

  pthread_mutex_lock(&injector_lock);
  latency_ms = injected_latency_ms;
  pthread_mutex_unlock(&injector_lock);

  return latency_ms;
}

void failure_inject_before_send(const char *peer,
                                uint32_t msg_type) {
  uint32_t latency_ms = failure_injector_get_latency();

  if (latency_ms == 0) {
    return;
  }

  printf("failure-injector: delaying message type=%u to %s by %u ms\n",
         msg_type,
         peer,
         latency_ms);

  struct timespec delay;

  delay.tv_sec = latency_ms / 1000;
  delay.tv_nsec = (long) (latency_ms % 1000) * 1000000L;

  nanosleep(&delay, NULL);
}