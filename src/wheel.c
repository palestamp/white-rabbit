
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stddef.h>
#include <time.h>

#include "wheel.h"

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

int64_t get_current_time(void) {
  struct timespec monotime;
  clock_gettime(CLOCK_MONOTONIC, &monotime); // XXX retrun code
  return to_micros(monotime);
}

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

int hwt_init(struct hwt *h) {
  struct timespec monotime;
  clock_gettime(CLOCK_MONOTONIC, &monotime); // XXX retrun code

  h->tick_time = to_micros(monotime);
  h->start_time = h->tick_time;
  h->tick = 0;

  init_timer_list(&h->pending);
  int i, j;
  for (i = 0; i < MAX_LEVEL; i++) {
    for (j = 0; j < SLOTS_SIZE; j++) {
      init_timer_list(&h->tvec[i][j]);
    }
  }
  return 1;
}

// accepts delay in microseconds
void hwt_schedule(struct hwt *h, int64_t delay, int id) {
  if (delay < MIN_TICK_INTERVAL) {
    delay = MIN_TICK_INTERVAL;
  }

  struct hwt_timer *t = timer_new(id);
  t->expire = h->tick_time + delay;

  list_add_tail(&t->list, &h->pending->timers.list);
}

void hwt_add_timer(struct hwt *h, struct hwt_timer *t) {
  // fprintf(stdout, "Adding timer: %d\n", t->id);
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

  struct hwt_timer_list *a = h->tvec[n][idx];
  init_timer_list(&h->tvec[n][idx]);

  struct list_head *pos, *p;
  struct hwt_timer *ht;
  list_for_each_safe(pos, p, &a->timers.list) {
    ht = list_entry(pos, struct hwt_timer, list);
    list_del(pos);
    hwt_add_timer(h, ht);
  }
  free(a);
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
  free(pending);
}

int run_framed_timers(struct hwt *h, int idx) {
  struct hwt_timer_list *ready = h->tvec[0][idx];
  init_timer_list(&h->tvec[0][idx]);

  int cnt = 0;
  struct list_head *pos, *p;
  struct hwt_timer *ht = NULL;
  list_for_each_safe(pos, p, &ready->timers.list) {
    ht = list_entry(pos, struct hwt_timer, list);
    list_del(pos);

    free(ht);
    cnt += 1;
  }
  free(ready);

  if (cnt)
    fprintf(stdout, "framed_run (emitted:%d) \n", cnt);
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
