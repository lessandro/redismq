#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ev.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>
#include "redismq.h"

static void rmq_send_blpop(struct rmq_context*);
static void rmq_connect(struct rmq_context*);

static void rmq_command_cb(redisAsyncContext *redis_ctx, void *r, void *data)
{
    redisReply *reply = (redisReply*)r;
    if (reply == NULL)
        return;

    if (reply->type != REDIS_REPLY_ARRAY)
        return;

    if (reply->elements != 2)
        return;

    struct rmq_context *ctx = (struct rmq_context*)redis_ctx->data;
    ctx->blpop_cb(reply->element[1]->str);

    rmq_send_blpop(ctx);
}

static void rmq_send_blpop(struct rmq_context *ctx)
{
    redisAsyncCommand(ctx->redis_ctx, rmq_command_cb, NULL, "BLPOP %s 0",
        ctx->key);
}

static void rmq_send_rpush(struct rmq_context *ctx)
{
    printf("send push\n");
}

static void rmq_timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    void *data = w->data;
    free(w);
    rmq_connect(data);
}

static void rmq_connect_wait(struct rmq_context *ctx)
{
    struct ev_timer *timer = malloc(sizeof(struct ev_timer));
    ev_timer_init(timer, rmq_timer_cb, 1., 0.);

    timer->data = ctx;

    ev_timer_start(EV_DEFAULT_ timer);
}

static void rmq_connect_cb(const redisAsyncContext *ctx, int status)
{
    if (status == REDIS_OK)
        return;

    printf("redis: %s\n", ctx->errstr);
    rmq_connect_wait(ctx->data);
}

static void rmq_disconnect_cb(const redisAsyncContext *ctx, int status)
{
    if (ctx->errstr)
        printf("redis: %s\n", ctx->errstr);

    rmq_connect(ctx->data);
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

    if (ctx->type == RMQ_BLPOP)
        rmq_send_blpop(ctx);
    else
        rmq_send_rpush(ctx);
}

void rmq_blpop(struct rmq_context *ctx, rmq_callback *blpop_cb)
{
    assert(ctx->type == 0);

    ctx->type = RMQ_BLPOP;
    ctx->blpop_cb = blpop_cb;

    rmq_connect(ctx);
}

void rmq_rpush(struct rmq_context *ctx, char *message)
{
    if (ctx->type == 0) {
        ctx->type = RMQ_RPUSH;
        STAILQ_INIT(&ctx->head);
    }

    assert(ctx->type == RMQ_RPUSH);

    struct rmq_message *msg = malloc(sizeof(struct rmq_message));
    msg->message = message;

    STAILQ_INSERT_TAIL(&ctx->head, msg, entries);

    // TODO: the rest of this function
}

void rmq_init(struct rmq_context *ctx, char *redis_host, int redis_port,
    int redis_db, char *key)
{
    ctx->type = 0;
    ctx->redis_host = redis_host;
    ctx->redis_port = redis_port;
    ctx->redis_db = redis_db;
    ctx->key = key;
    ctx->redis_ctx = NULL;
}
