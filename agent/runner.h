#ifndef __RUNNER_H
#define __RUNNER_H
#include<xjr-helpers.h>
#include<xjr-node.h>
typedef struct runnerItem_s runnerItem;
struct runnerItem_s {
    char *data;
    int len;
    runnerItem *next;
};

void run_some_items();
void runner_handle_item( xjr_node *item, char *itemIdStr );
void runner_handle_runnerItem( runnerItem *item );
void runner_queue_incoming_bytes( int bytes, char *buf );
void runner_loop();
void join_runner();
void spawn_runner( volatile int *running );
#endif