#include "output.h"
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>

output *output__new() {
    output *self = ( output * ) calloc( 1, sizeof( output ) );
    return self;
}

void output__delete( output *self ) {
    if( self->socket_str ) free( self->socket_str );
}

output *setup_output( xjr_node *item, int nntype ) {
    output *cur = output__new();
    int send_timeout = 200;
    int recv_timeout = 200; // Note that this recv timeout does not make send for request reply as those can take longer
    cur->socket_str = xjr_node__get_valuez( item, "socket", 6 );
    if( cur->socket_str ) {
        int socket_id = nn_socket( AF_SP, nntype );
        int bind_res = nn_connect( socket_id, cur->socket_str );
        if( bind_res < 0 ) {
            fprintf( stderr, "Error binding %s\n", cur->socket_str );
        }
        else { // success
            cur->socket_id = socket_id;
        }   
        nn_setsockopt( socket_id, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof( send_timeout ) );
        nn_setsockopt( socket_id, NN_SOL_SOCKET, NN_RCVTIMEO, &recv_timeout, sizeof( recv_timeout ) );
    }

    return cur;
}