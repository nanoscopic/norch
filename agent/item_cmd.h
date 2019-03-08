#ifndef __ITEM_CMD_H
#define __ITEM_CMD_H
#include<xjr-node.h>
char *item_cmd( xjr_node *item, char *itemIdStr );
int run_cmd( char **args, int argLen, long int *outLen, char **outp, long int *errLen, char **errp, long int *inLen, char *in, int *errLevel );
#endif