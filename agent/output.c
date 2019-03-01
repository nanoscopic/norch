#include "output.h"
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include"misc.h"

output *output__new() {
    output *self = ( output * ) calloc( 1, sizeof( output ) );
    return self;
}

void output__delete( output *self ) {
    if( self->socket_str ) free( self->socket_str );
}

output *setup_output( xjr_node *item, int nntype, int send_timeout, int recv_timeout ) {
    printf("Start of setup_output\n");
    xjr_node__dump( item, 20 );
    output *cur = output__new();
    cur->socket_str = xjr_node__get_valuez( item, "socket", 6 );
    printf("  socket str = %s\n", cur->socket_str );
    
    if( !cur->socket_str ) {
        xjr_node__dump( item, 20 );
        printf("Node passed to setup_output has no socket\n");
        exit(0);
    }
    if( cur->socket_str ) {
        cur->socket_id = make_nn_socket( cur->socket_str, connect, nntype, send_timeout, recv_timeout );
    }

    return cur;
}