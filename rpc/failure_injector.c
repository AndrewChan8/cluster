#include "failure_injector.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static pthread_mutex_t injector_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t injected_latency_ms = 0;
static uint32_t injected_drop_percent = 0;

void failure_injector_init(void) {
  pthread_mutex_lock(&injector_lock);
  injected_latency_ms = 0;
  injected_drop_percent = 0;

  srand((unsigned int) time(NULL));
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

int failure_inject_before_send(const char *peer,
                               uint32_t msg_type) {
  uint32_t latency_ms = failure_injector_get_latency();
  uint32_t drop_percent = failure_injector_get_drop();

  if (drop_percent > 0) {
    uint32_t roll = (uint32_t) (rand() % 100);

    if (roll < drop_percent) {
      printf("failure-injector: dropping message type=%u to %s drop=%u%%\n",
             msg_type,
             peer,
             drop_percent);
      return -1;
    }
  }

  if (latency_ms > 0) {
    struct timespec delay;

    printf("failure-injector: delaying message type=%u to %s by %u ms\n",
           msg_type,
           peer,
           latency_ms);

    delay.tv_sec = latency_ms / 1000;
    delay.tv_nsec = (long) (latency_ms % 1000) * 1000000L;

    nanosleep(&delay, NULL);
  }

  return 0;
}

void failure_injector_set_drop(uint32_t drop_percent) {
  if (drop_percent > 100) {
    drop_percent = 100;
  }

  pthread_mutex_lock(&injector_lock);
  injected_drop_percent = drop_percent;
  pthread_mutex_unlock(&injector_lock);

  printf("failure-injector: drop set to %u%%\n", drop_percent);
}

uint32_t failure_injector_get_drop(void) {
  uint32_t drop_percent;

  pthread_mutex_lock(&injector_lock);
  drop_percent = injected_drop_percent;
  pthread_mutex_unlock(&injector_lock);

  return drop_percent;
}