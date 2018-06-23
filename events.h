#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <util/atomic.h>

struct EventFlags {
  bool new_1s_cycle : 1;
};
extern volatile EventFlags Events;

extern volatile uint32_t unix_time;

static inline uint32_t get_unix_time(void) {
  uint32_t ret;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    ret = unix_time;
  }
  return ret;
}

static inline void set_unix_time(uint32_t t) {
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    unix_time = t;
  }
}

#endif
