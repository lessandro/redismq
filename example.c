/*-
 * Copyright (c) 2013, Lessandro Mariano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
