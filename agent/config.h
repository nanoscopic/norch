#ifndef __CONFIG_H
#define __CONFIG_H
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<xjr-machine.h>
typedef struct config_s {
    char *director_broadcast_socket;
    char *director_requests_socket;
    char *director_results_socket;
    char *input_socket;
} config;
config *config__new();
config *read_config( xjr_node *configRoot );
void print_config( config *conf );
#endif