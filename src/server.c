#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <uv.h>

#include "wheel.h"

#define DEFAULT_PORT 7000
#define DEFAULT_BACKLOG 128

uv_loop_t *loop;
struct sockaddr_in addr;
uv_mutex_t wheel_mtx;
struct hwt ghwt;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void timer_dump(struct hwt_timer *t) { fprintf(stdout, "emit:%s\n", t->id); }

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t *)req;
    free(wr->buf.base);
    free(wr);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_close(uv_handle_t *handle) { free(handle); }

void echo_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free_write_req(req);
}

struct input_timer {
    int64_t tbase;
    tr_e res;
    char *id;
};

struct input_timer *parse_timer(struct input_timer *it, const char *input) {
    if (!input) {
        return NULL;
    }
    char *c = (char *)input;

    for (; *c && *c != ' '; c++)
        ;

    char *id = malloc(sizeof(char) * (c - input + 1));
    strncpy(id, input, c - input);
    id[c - input] = '\0';
    it->id = id;

    c++;
    char *c2 = c;
    for (; *c2 && isdigit(*c2); c2++)
        ;

    char *delay = malloc(sizeof(char) * (c2 - c + 1));
    strncpy(delay, c, c2 - c);
    delay[c2 - c] = '\0';

    fprintf(stderr, "accept:%s:%s\n", id, delay);

    it->tbase = atoll(delay);
    it->res = MILLISECOND;

    return it;
}

void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        write_req_t *req = (write_req_t *)malloc(sizeof(write_req_t));

        uv_mutex_lock(&wheel_mtx);
        struct input_timer it;
        parse_timer(&it, buf->base);
        hwt_schedule(&ghwt, to_micro(it.tbase, it.res), it.id, timer_dump);
        uv_mutex_unlock(&wheel_mtx);

        req->buf = uv_buf_init(buf->base, nread);
        uv_write((uv_write_t *)req, client, &req->buf, 1, echo_write);
        return;
    }
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)client, on_close);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        // error!
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_buffer, echo_read);
    } else {
        uv_close((uv_handle_t *)client, on_close);
    }
}

void hwt_loop(void *arg) {
    int64_t ti = 1e6 / 100;
    int64_t last = get_current_time();
    while (1) {
        uv_mutex_lock(&wheel_mtx);
        int64_t curr = get_current_time();
        hwt_tick(&ghwt, curr - last);
        last = curr;
        int64_t cost = get_current_time() - curr;
        uv_mutex_unlock(&wheel_mtx);
        if (cost < ti) {
            usleep(ti - cost);
        }
    }
}

int main() {
    if (uv_mutex_init(&wheel_mtx) < 0) {
        perror("wheel_mutex");
    }

    if (!hwt_init(&ghwt)) {
        perror("hwt");
    }

    uv_thread_t hwt_thread_id;
    uv_thread_create(&hwt_thread_id, hwt_loop, NULL);

    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    int r =
        uv_listen((uv_stream_t *)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);
}
