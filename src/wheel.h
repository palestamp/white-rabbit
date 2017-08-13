#ifndef _WR_WHEEL_H
#define _WR_WHEEL_H

#include "list.h"

typedef enum time_resolution { MINUTE, SECOND, MILLISECOND, MICROSECOND } tr_e;

struct hwt_timer {
  struct list_head list;
  int id;
  int64_t expire;
};

struct hwt_timer_list {
  struct hwt_timer timers;
};

#endif