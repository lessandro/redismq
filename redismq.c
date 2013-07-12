#include <stdio.h>
#include <stdlib.h>
#include <ev.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>
#include "redismq.h"

static void rmq_send_blpop(redisAsyncContext *);
static void rmq_connect(rmq_config *cfg);

static void rmq_command_cb(redisAsyncContext *ctx, void *r, void *privdata)
{
    redisReply *reply = (redisReply*)r;
    if (reply == NULL)
        return;

    if (reply->type != REDIS_REPLY_ARRAY)
        return;

    if (reply->elements != 2)
        return;

    rmq_config *cfg = (rmq_config*)ctx->data;
    cfg->fn(reply->element[1]->str);

    rmq_send_blpop(ctx);
}

static void rmq_send_blpop(redisAsyncContext *ctx)
{
    rmq_config *cfg = (rmq_config*)ctx->data;
    redisAsyncCommand(ctx, rmq_command_cb, NULL, "BLPOP %s 0", cfg->key);
}

static void rmq_timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    void *data = w->data;
    free(w);
    rmq_connect(data);
}

static void rmq_connect_wait(rmq_config *cfg)
{
    struct ev_timer *timer = malloc(sizeof(struct ev_timer));
    ev_timer_init(timer, rmq_timer_cb, 1., 0.);

    timer->data = cfg;

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

static void rmq_connect(rmq_config *cfg)
{
    redisAsyncContext *ctx = redisAsyncConnect(
        cfg->redis_host,
        cfg->redis_port);

    if (ctx->err) {
        rmq_connect_wait(cfg);
        return;
    }

    cfg->ctx = ctx;
    ctx->data = (void*)cfg;

    redisLibevAttach(EV_DEFAULT_ ctx);
    redisAsyncSetConnectCallback(ctx, rmq_connect_cb);
    redisAsyncSetDisconnectCallback(ctx, rmq_disconnect_cb);
    redisAsyncCommand(ctx, NULL, NULL, "SELECT %d", cfg->redis_db);

    rmq_send_blpop(ctx);
}

void rmq_blpop(rmq_config *cfg)
{
    rmq_connect(cfg);
}

void rmq_rpush(rmq_config *cfg, char *msg)
{
    if (!cfg->ctx) {
        rmq_connect(cfg);
    }
}
