#ifndef REDISMQ_H
#define REDISMQ_H

#include <sys/queue.h>

typedef void (rmq_callback)(char*);

struct rmq_message {
    LIST_ENTRY(rmq_message) pointers;
};

typedef struct rmq_config {
    char *key;
    rmq_callback *fn;
    char *redis_host;
    int redis_port;
    int redis_db;
    void *ctx;
} rmq_config;

void rmq_blpop(struct rmq_config*);

void rmq_rpush(struct rmq_config*, char *msg);

#endif
