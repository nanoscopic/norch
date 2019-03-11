#ifndef __ITEM_CMD_H
#define __ITEM_CMD_H
#include<xjr-node.h>
char *item_cmd( xjr_node *item, char *itemIdStr );
typedef struct cmd_res_s {
    long int outLen;
    char *out;
    long int errLen;
    char *err;
    int errorLevel;
} cmd_res;
cmd_res *run_cmd( char **args, int argLen, long int *inLen, char *in );
#endif