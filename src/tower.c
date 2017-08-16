#include <stdlib.h>

#include "tower.h"

int tower_api_create(struct ev_loop *evl) {}
void tower_delete_ev_loop();

void tower_create_event();
void tower_delete_event();

/* Return number of events polled. */
int tower_evl_poll(struct ev_loop *evl) {}

/* Create event loop with `setsize` size */
struct ev_loop *tower_create_ev_loop(int setsize) {
    struct ev_loop *evl;
    int i;

    if ((evl = malloc(sizeof(*evl))) == NULL)
        goto err;

    if ((evl->events = malloc(sizeof(struct fd_event) * setsize)) == NULL)
        goto err;

    evl->setsize = setsize;
    evl->terminated = 0;
    evl->maxfd = -1;

    if (tower_api_create(evl) == -1)
        goto err;

    for (i = 0; i < setsize; i++) {
        evl->events[i].flags = T_EV_NONE;
    }
    return evl;

err:
    if (evl) {
        // XXX is it safe, what if evl malloc failed?
        free(evl->events);
        free(evl);
    }
    return NULL;
};

int tower_process_events(struct ev_loop *evl) {
    int i, numevents;
    if (evl->maxfd < 0) {
        return 0;
    }

    numevents = tower_evl_poll(evl);

    for (i = 0; i < numevents; i++) {
        struct fd_event *fdev = &evl->events[i];
        if (fdev->flags & T_EV_READ) {
            fdev->read_fn(fdev);
        }
        // XXX what should happen in case of T_EV_WRITE | T_EV_READ event?
        if (fdev->flags & T_EV_WRITE) {
            fdev->write_fn(fdev);
        }
    }

    return numevents;
}

void tower_run_ev_loop(struct ev_loop *evl) {
    evl->terminated = 0;
    while (!evl->terminated) {
        tower_process_events(evl);
    }
};