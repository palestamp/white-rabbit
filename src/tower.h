#ifndef _WR_TOWER_H
#define _WR_TOWER_H


#define T_EV_NONE 0
#define T_EV_READ 1
#define T_EV_WRITE 2

struct fd_event;

typedef void fdev_fn(struct fd_event *);

/* event loop fd event */
struct fd_event {
    int flags;
    fdev_fn *read_fn;
    fdev_fn *write_fn;
};

/* event loop */
struct ev_loop {
    int terminated;
    int maxfd;
    int setsize;
    struct fd_event *events;
    void *poll_api;
};

#endif
