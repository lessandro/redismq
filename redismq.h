#ifndef REDISMQ_H
#define REDISMQ_H

typedef void (rmq_callback)(char*);

typedef struct rmq_config {
    char *key;
    rmq_callback *fn;
    char *redis_host;
    int redis_port;
    int redis_db;
} rmq_config;

void rmq_blpop(struct rmq_config*);

#endif
