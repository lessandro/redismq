#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include "redismq.h"

static void timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    printf("sending...\n");
    rmq_rpush(w->data, "test");
}

static void blpop_cb(char *msg)
{
    printf("received: %s\n", msg);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    // rpush timer

    rmq_config push_config = {
        .key = "list",
        .redis_host = "127.0.0.1",
        .redis_port = 6379,
        .redis_db = 0
    };

    struct ev_timer timer;
    ev_timer_init(&timer, timer_cb, 0., 1.);
    timer.data = &push_config;
    ev_timer_start(EV_DEFAULT_ &timer);

    // blpop handler

    rmq_config pop_config = {
        .key = "list",
        .fn = blpop_cb,
        .redis_host = "127.0.0.1",
        .redis_port = 6379,
        .redis_db = 0
    };

    rmq_blpop(&pop_config);

    ev_loop(EV_DEFAULT_ 0);

    return 0;
}
