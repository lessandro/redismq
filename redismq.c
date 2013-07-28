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

// for vasprintf / strdup
#define _GNU_SOURCE 1

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ev.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>
#include "redismq.h"

#if HIREDIS_MAJOR == 0 && HIREDIS_MINOR < 11
#error hiredis >= 0.11 required
#endif

// message

struct rmq_message *rmq_message_new(const char *message)
{
    struct rmq_message *msg = malloc(sizeof(struct rmq_message));
    msg->message = strdup(message);
    return msg;
}

void rmq_message_free(struct rmq_message *msg)
{
    free(msg->message);
    free(msg);
}

// blpop implementation

static void rmq_blpop_loop(struct rmq_context*);
static void rmq_connect(struct rmq_context*);

static void rmq_blpop_cb(redisAsyncContext *redis_ctx, void *r, void *data)
{
    redisReply *reply = (redisReply*)r;

    if (reply == NULL)
        return;

    assert(reply->type == REDIS_REPLY_ARRAY);
    assert(reply->elements == 2);

    struct rmq_context *ctx = (struct rmq_context*)redis_ctx->data;
    ctx->blpop_cb(reply->element[1]->str);

    rmq_blpop_loop(ctx);
}

static void rmq_blpop_loop(struct rmq_context *ctx)
{
    redisAsyncCommand(ctx->redis_ctx, rmq_blpop_cb, NULL, "BLPOP %b 0",
        ctx->key, strlen(ctx->key));
}

// rpush implementation

static void rmq_rpush_cb(redisAsyncContext *redis_ctx, void *r, void *data)
{
    redisReply *reply = (redisReply*)r;

    if (reply == NULL)
        return;

    assert(reply->type == REDIS_REPLY_INTEGER);

    struct rmq_context *ctx = (struct rmq_context*)redis_ctx->data;
    struct rmq_message *msg = STAILQ_FIRST(&ctx->head);
    STAILQ_REMOVE_HEAD(&ctx->head, entries);
    rmq_message_free(msg);
}

static void rmq_rpush_message(struct rmq_context *ctx, struct rmq_message *msg)
{
    redisAsyncCommand(ctx->redis_ctx, rmq_rpush_cb, msg, "RPUSH %b %b",
        ctx->key, strlen(ctx->key), msg->message, strlen(msg->message));
}

static void rmq_rpush_all(struct rmq_context *ctx)
{
    struct rmq_message *m;
    STAILQ_FOREACH(m, &ctx->head, entries) {
        rmq_rpush_message(ctx, m);
    }
}

// reconnect implementation

static void rmq_timer_cb(EV_P_ struct ev_timer *timer, int revents)
{
    void *data = timer->data;
    free(timer);
    rmq_connect(data);
}

static void rmq_connect_wait(struct rmq_context *ctx)
{
    struct ev_timer *timer = malloc(sizeof(struct ev_timer));
    ev_timer_init(timer, rmq_timer_cb, 1., 0.);

    timer->data = ctx;

    ev_timer_start(EV_DEFAULT_ timer);
}

static void rmq_connect_cb(const redisAsyncContext *redis_ctx, int status)
{
    struct rmq_context *ctx = (struct rmq_context*)redis_ctx->data;

    if (status != REDIS_OK) {
        if (redis_ctx->errstr)
            printf("redis: %s\n", redis_ctx->errstr);

        rmq_connect_wait(ctx);
        return;
    }

    ctx->connected = 1;

    switch (ctx->type) {
        case RMQ_BLPOP:
            rmq_blpop_loop(ctx);
            break;
        case RMQ_RPUSH:
            rmq_rpush_all(ctx);
            break;
        default:
            assert(0);
    }
}

static void rmq_disconnect_cb(const redisAsyncContext *redis_ctx, int status)
{
    if (redis_ctx->errstr)
        printf("redis: %s\n", redis_ctx->errstr);

    struct rmq_context *ctx = (struct rmq_context*)redis_ctx->data;
    ctx->connected = 0;
    rmq_connect(ctx);
}

static void rmq_connect(struct rmq_context *ctx)
{
    redisAsyncContext *redis_ctx = redisAsyncConnect(
        ctx->redis_host,
        ctx->redis_port);

    if (redis_ctx->err) {
        rmq_connect_wait(ctx);
        return;
    }

    ctx->redis_ctx = (void*)redis_ctx;
    redis_ctx->data = (void*)ctx;

    redisLibevAttach(EV_DEFAULT_ redis_ctx);
    redisAsyncSetConnectCallback(redis_ctx, rmq_connect_cb);
    redisAsyncSetDisconnectCallback(redis_ctx, rmq_disconnect_cb);
    redisAsyncCommand(redis_ctx, NULL, NULL, "SELECT %d", ctx->redis_db);
}

// interface

void rmq_blpop(struct rmq_context *ctx, rmq_callback *blpop_cb)
{
    assert(ctx->type == RMQ_NONE);

    ctx->type = RMQ_BLPOP;
    ctx->blpop_cb = blpop_cb;

    rmq_connect(ctx);
}

void rmq_rpush(struct rmq_context *ctx, const char *message)
{
    if (ctx->type == RMQ_NONE) {
        ctx->type = RMQ_RPUSH;
        STAILQ_INIT(&ctx->head);

        rmq_connect(ctx);
    }

    assert(ctx->type == RMQ_RPUSH);

    struct rmq_message *msg = rmq_message_new(message);
    STAILQ_INSERT_TAIL(&ctx->head, msg, entries);

    if (ctx->connected)
        rmq_rpush_message(ctx, msg);
}

void rmq_rpushf(struct rmq_context *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *buf;
    vasprintf(&buf, format, args);
    rmq_rpush(ctx, buf);
    free(buf);
    va_end(args);
}

void rmq_init(struct rmq_context *ctx, const char *redis_host, int redis_port,
    int redis_db, const char *key)
{
    ctx->type = RMQ_NONE;
    ctx->redis_host = strdup(redis_host);
    ctx->redis_port = redis_port;
    ctx->redis_db = redis_db;
    ctx->key = strdup(key);
    ctx->redis_ctx = NULL;
    ctx->connected = 0;
}
