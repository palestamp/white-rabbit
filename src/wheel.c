
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stddef.h>
#include <time.h>
#include <unistd.h>

#include "list.h"

#define MAX_LEVEL 5

#define SLOTS 6
#define SLOTS_SIZE (1 << SLOTS)
#define SLOTS_MASK (SLOTS_SIZE - 1)

#define MIN_TICK_INTERVAL 1e3  // 1mcs

typedef enum time_resolution { MINUTE, SECOND, MILLISECOND, MICROSECOND } tr_e;

int64_t to_micro(int64_t v, tr_e res) {
  switch (res) {
    case MICROSECOND:
      return v;
    case MILLISECOND:
      return v * 1e3;
    case SECOND:
      return v * 1e6;
    case MINUTE:
      return v * 1e6 * 60;
  }
}

int64_t to_micros(struct timespec t) {
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

int64_t get_current_time() {
  struct timespec monotime;
  clock_gettime(CLOCK_MONOTONIC, &monotime);  // XXX retrun code
  return to_micros(monotime);
}

struct hwt_timer {
  struct list_head list;
  int id;
  int64_t expire;
};

struct hwt_timer_list {
  struct hwt_timer timers;
};

void init_timer_list(struct hwt_timer_list **tl) {
  struct hwt_timer_list *tll = calloc(1, sizeof(struct hwt_timer_list));
  INIT_LIST_HEAD(&tll->timers.list);

  *tl = tll;
}

struct hwt_timer *timer_new(int id) {
  struct hwt_timer *t = calloc(1, sizeof(struct hwt_timer));
  t->id = id;
  t->expire = 0;
  return t;
}

struct hwt {
  int64_t tick_time;
  int64_t start_time;
  int64_t tick;

  struct hwt_timer_list *tvec[MAX_LEVEL][SLOTS_SIZE];
  struct hwt_timer_list *pending;
};

int hwt_init(struct hwt *h) {
  struct timespec monotime;
  clock_gettime(CLOCK_MONOTONIC, &monotime);  // XXX retrun code

  h->tick_time = to_micros(monotime);
  h->start_time = h->tick_time;
  h->tick = 0;

  init_timer_list(&h->pending);
  int i, j;
  for (i = 0; i < MAX_LEVEL; i++) {
    for (j = 0; j < SLOTS_SIZE; j++) {
      h->tvec[i][j] = NULL;
      init_timer_list(&h->tvec[i][j]);
    }
  }
  return 1;
}

// accepts delay in microseconds
void hwt_schedule(struct hwt *h, int64_t delay, int id) {
  fprintf(stderr, "hwt_shedule (delay:%lld, id:%d)\n", delay, id);
  if (delay < MIN_TICK_INTERVAL) {
    delay = MIN_TICK_INTERVAL;
  }

  struct hwt_timer *t = timer_new(id);
  t->expire = h->tick_time + delay;

  list_add_tail(&t->list, &h->pending->timers.list);
}

void hwt_add_timer(struct hwt *h, struct hwt_timer *t) {
  // fprintf(stderr, "Adding timer: %d\n", t->id);
  int ticks = (t->expire - h->tick_time) / MIN_TICK_INTERVAL;

  if (ticks < 0) {
    ticks = 0;
  }

  int idx = h->tick + ticks;
  int level = 0;

  if (ticks < SLOTS_SIZE) {
    idx = idx & SLOTS_MASK;
    level = 0;
  } else if (ticks < (1 << (2 * SLOTS))) {
    idx = (idx >> SLOTS) & SLOTS_MASK;
    level = 1;
  } else if (ticks < (1 << (3 * SLOTS))) {
    idx = (idx >> (2 * SLOTS)) & SLOTS_MASK;
    level = 2;
  } else if (ticks < (1 << (4 * SLOTS))) {
    idx = (idx >> (3 * SLOTS)) & SLOTS_MASK;
    level = 3;
  } else {
    idx = (idx >> (4 * SLOTS)) & SLOTS_MASK;
    level = 4;
  }

  list_add_tail(&t->list, &h->tvec[level][idx]->timers.list);
}

int cascade(struct hwt *h, int n) {
  int idx = (h->tick >> (n * SLOTS)) & SLOTS_MASK;
  //fprintf(stderr, "cascade (level:%d, idx:%d)\n", n, idx);

  struct hwt_timer_list *a = h->tvec[n][idx];
  init_timer_list(&h->tvec[n][idx]);

  struct list_head *pos, *p;
  struct hwt_timer *ht;
  list_for_each_safe(pos, p, &a->timers.list) {
    ht = list_entry(pos, struct hwt_timer, list);
    list_del(pos);
    hwt_add_timer(h, ht);
  }
  return idx;
}

void wheel_advance(struct hwt *h, int32_t idx) {
  if (idx == 0 && (cascade(h, 1) == 0) && (cascade(h, 2) == 0)) {
    cascade(h, 3);
  }
}

void add_pending_timers(struct hwt *h) {
  struct hwt_timer_list *pending = h->pending;
  h->pending = NULL;
  init_timer_list(&h->pending);

  struct list_head *pos, *p;
  struct hwt_timer *ht = NULL;
  list_for_each_safe(pos, p, &pending->timers.list) {
    ht = list_entry(pos, struct hwt_timer, list);
    list_del(pos);
    hwt_add_timer(h, ht);
  }
}

int run_framed_timers(struct hwt *h, int idx) {
  struct hwt_timer_list *ready = h->tvec[0][idx];
  init_timer_list(&h->tvec[0][idx]);

  int cnt = 0;
  struct list_head *pos = NULL;
  struct hwt_timer *ht = NULL;
  list_for_each(pos, &ready->timers.list) {
    ht = list_entry(pos, struct hwt_timer, list);
    fprintf(stderr, "framed_run (id:%d)\n", ht->id);
    cnt += 1;
  }

  return cnt;
}

int hwt_tick(struct hwt *h, int diff) {
  int ticks = diff / MIN_TICK_INTERVAL;
  int rc = 0;

  add_pending_timers(h);

  for (; ticks > 0; ticks--) {
    int32_t idx = h->tick & SLOTS_MASK;

    wheel_advance(h, idx);
    rc += run_framed_timers(h, idx);

    h->tick++;
    h->tick_time += MIN_TICK_INTERVAL;
  }
  return rc;
}

int main() {
  struct hwt hwt;

  if (!hwt_init(&hwt)) {
    perror("hwt");
  }

  hwt_schedule(&hwt, to_micro(1, SECOND), 11111);

  hwt_schedule(&hwt, to_micro(2, SECOND) + to_micro(500, MILLISECOND), 22222);
  /*
  hwt_schedule(&hwt, to_micro(5, SECOND), 22223);
  hwt_schedule(&hwt, to_micro(7, SECOND), 33333);
  hwt_schedule(&hwt, to_micro(10, SECOND), 44444);
  */

  int64_t ti = 1e6 / 10;
  int64_t last = get_current_time();
  while (1) {
    int64_t curr = get_current_time();
    if (hwt_tick(&hwt, curr - last)) {
      hwt_schedule(&hwt, to_micro(4, SECOND), 31313);
    }
    last = curr;
    int64_t cost = get_current_time() - curr;
    if (cost < ti) {
      usleep(ti - cost);
    }
  }
}