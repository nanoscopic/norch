#include"config.h"
#include<stdlib.h>
config *read_config( xjr_node *configRoot ) {
    xjr_node *broadcast_node = xjr_node__get( configRoot, "broadcast", 9 );
    if( !broadcast_node ) { fprintf(stderr,"config has no broadcast node\n"); exit(1); }
    
    xjr_node *requests_node  = xjr_node__get( configRoot, "director_requests", 17 );
    if( !requests_node ) { fprintf(stderr,"config has no director_requests node\n"); exit(1); }
    
    xjr_node *results_node = xjr_node__get( configRoot, "director_results" , 16 );
    if( !results_node ) { fprintf(stderr,"Configroot has no director_results node\n"); exit(1); }
    
    xjr_node *input_node = xjr_node__get( configRoot, "input" , 5 );
    if( !input_node ) { fprintf(stderr,"Configroot has no input node\n"); exit(1); }
    
    config *conf = config__new();
    conf->director_broadcast_socket = xjr_node__get_valuez( broadcast_node, "socket", 6 );
    conf->director_requests_socket  = xjr_node__get_valuez( requests_node , "socket", 6 );
    conf->director_results_socket   = xjr_node__get_valuez( results_node  , "socket", 6 );
    conf->input_socket              = xjr_node__get_valuez( input_node    , "socket", 6 );
    return conf;
}

config *config__new() {
    config *self = ( config * ) calloc( 1, sizeof( config ) );
    return self;
}

void print_config( config *conf ) {
    printf("Config:\n"
           "  Director broadcast socket: %s\n"
           "  Director requests  socket: %s\n"
           "  Director results   socket: %s\n"
           "  Input              socket: %s\n",
           conf->director_broadcast_socket,
           conf->director_requests_socket,
           conf->director_results_socket,
           conf->input_socket );
}