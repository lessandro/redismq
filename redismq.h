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

#ifndef REDISMQ_H
#define REDISMQ_H

#include <sys/queue.h>

#define RMQ_NONE 0
#define RMQ_BLPOP 1
#define RMQ_RPUSH 2

typedef void (rmq_callback)(const char *message);

struct rmq_message {
    char *message;
    STAILQ_ENTRY(rmq_message) entries;
};

struct rmq_context {
    // blpop or rpush
    int type;

    // redis config
    const char *redis_host;
    int redis_port;
    int redis_db;

    // redis key
    const char *key;

    // pointer to redis context
    void *redis_ctx;
    int connected;

    // callback for blpop
    rmq_callback *blpop_cb;

    // message queue for rpush
    STAILQ_HEAD(rmq_message_head, rmq_message) head;
};

void rmq_init(struct rmq_context *ctx, const char *redis_host, int redis_port,
    int redis_db, const char *key);

void rmq_blpop(struct rmq_context *ctx, rmq_callback *blpop_cb);

void rmq_rpush(struct rmq_context *ctx, const char *message);

void rmq_rpushf(struct rmq_context *ctx, const char *format, ...);

#endif
