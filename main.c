#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include "redismq.h"

void blpop_callback(char *msg)
{
    printf("received: %s\n", msg);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    rmq_config config = {
        .key = "list",
        .fn = blpop_callback,
        .redis_host = "127.0.0.1",
        .redis_port = 6379,
        .redis_db = 0
    };

    rmq_blpop(&config);

    ev_loop(EV_DEFAULT_ 0);

    return 0;
}
