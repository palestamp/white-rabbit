#ifndef _WR_WHEEL_H
#define _WR_WHEEL_H

#include "list.h"

#define MAX_LEVEL 5

#define SLOTS 6
#define SLOTS_SIZE (1 << SLOTS)
#define SLOTS_MASK (SLOTS_SIZE - 1)

#define MIN_TICK_INTERVAL 1e3 // 1mcs

typedef enum time_resolution { MINUTE, SECOND, MILLISECOND, MICROSECOND } tr_e;

struct hwt_timer {
  struct list_head list;
  int id;
  int64_t expire;
};

struct hwt_timer_list {
  struct hwt_timer timers;
};

struct hwt {
  int64_t tick_time;
  int64_t start_time;
  int64_t tick;

  struct hwt_timer_list *tvec[MAX_LEVEL][SLOTS_SIZE];
  struct hwt_timer_list *pending;
};

int64_t get_current_time(void);
int hwt_init(struct hwt *h);
void hwt_schedule(struct hwt *h, int64_t delay, int id);
int64_t to_micro(int64_t v, tr_e res);
int hwt_tick(struct hwt *h, int diff);

#endif
