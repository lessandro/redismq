#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ev.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libev.h>
#include "redismq.h"

static void get_next_item(redisAsyncContext *);

static void on_command(redisAsyncContext *ctx, void *r, void *privdata)
{
    redisReply *reply = (redisReply*)r;
    if (reply == NULL)
        return;

    if (reply->type != REDIS_REPLY_ARRAY)
        return;

    if (reply->elements != 2)
        return;

    rmq_config *config = (rmq_config*)ctx->data;
    config->fn(reply->element[1]->str);

    get_next_item(ctx);
}

static void get_next_item(redisAsyncContext *ctx)
{
    rmq_config *config = (rmq_config*)ctx->data;
    redisAsyncCommand(ctx, NULL, NULL, "SELECT %d", config->redis_db);
    redisAsyncCommand(ctx, on_command, NULL, "BLPOP %s 0", config->key);
}

static void on_connect(const redisAsyncContext *ctx, int status)
{
    if (status != REDIS_OK) {
        printf("Error: %s\n", ctx->errstr);
        return;
    }
    printf("Connected...\n");
}

static void on_disconnect(const redisAsyncContext *ctx, int status)
{
    if (status != REDIS_OK) {
        printf("Error: %s\n", ctx->errstr);
        return;
    }
    printf("Disconnected...\n");
}

void rmq_blpop(struct rmq_config *config)
{
    redisAsyncContext *ctx;
    ctx = redisAsyncConnect(config->redis_host, config->redis_port);
    ctx->data = (void*)config;
    redisLibevAttach(EV_DEFAULT_ ctx);
    redisAsyncSetConnectCallback(ctx, on_connect);
    redisAsyncSetDisconnectCallback(ctx, on_disconnect);
    get_next_item(ctx);
}
