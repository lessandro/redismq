#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include "redismq.h"

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_DB 0
#define KEY "list"

static int n = 0;

static void timer_cb(EV_P_ struct ev_timer *timer, int revents)
{
    char str[10];
    sprintf(str, "test %d", n++);

    printf("sending %s\n", str);
    rmq_rpush(timer->data, str);
}

static void blpop_cb(char *msg)
{
    printf("received: %s\n", msg);
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    // rpush timer

    struct rmq_context push;
    rmq_init(&push, REDIS_HOST, REDIS_PORT, REDIS_DB, KEY);

    struct ev_timer timer;
    ev_timer_init(&timer, timer_cb, 0., .4);
    timer.data = &push;
    ev_timer_start(EV_DEFAULT_ &timer);

    // blpop handler

    struct rmq_context pop;
    rmq_init(&pop, REDIS_HOST, REDIS_PORT, REDIS_DB, KEY);
    rmq_blpop(&pop, blpop_cb);

    // start libev event loop

    ev_loop(EV_DEFAULT_ 0);

    return 0;
}
