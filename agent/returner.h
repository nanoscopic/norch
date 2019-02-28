#ifndef __RETURNER_H
#define __RETURNER_H
#include<xjr-node.h>
typedef struct returnerItem_s returnerItem;
struct returnerItem_s {
    char *data;
    int len;
    returnerItem *next;
};
returnerItem *returnerItem__add( char *data, int len );
returnerItem *returnerItem__pop();
void returner_loop();
void returner_handle_item( returnerItem *item );
void returner_queue_result( char *result );
void join_returner();
void spawn_returner( volatile int *running );
void returner_setup_output( xjr_node *configRoot );
#endif