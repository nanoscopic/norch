#ifndef __OUTPUT_H
#define __OUTPUT_H
#include<xjr-helpers.h>
#include<xjr-node.h>
typedef struct output_s output;
struct output_s {
    char *socket_str;
    int socket_id;
};
void output__delete( output *self );
output *setup_output( xjr_node *item, int nntype );
#endif