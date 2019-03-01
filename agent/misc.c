#include<stdio.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include<stdlib.h>
#include<string.h>
#include"misc.h"

char *decode_err( int err ) {
    char *buf;
    if( err == EBADF ) { return strdup("EBADF"); }
    if( err == ENOTSUP ) { return strdup("ENOTSUP"); }
    if( err == EFSM ) { return strdup("EFSM"); }
    if( err == EAGAIN ) { return strdup("EAGAIN"); }
    if( err == EINTR ) { return strdup("EINTR"); }
    if( err == ETIMEDOUT ) { return strdup("ETIMEDOUT"); }
    buf = malloc( 100 );// char[100];
    sprintf(buf,"Err number: %i", err );
    return buf;
}

int make_nn_socket( char *address, enum bindtype btype, int type, int send_timeout, int recv_timeout ) {
    int socket = nn_socket( AF_SP, type );
    int bind_res;
    
    if     ( btype == bind    ) bind_res = nn_bind   ( socket, address );
    else if( btype == connect ) bind_res = nn_connect( socket, address );
    else {
        // can never happen
        bind_res = -1;
    }
    if( bind_res < 0 ) {
        fprintf( stderr, "Error binding %s\n", address );
        return -1;
    }
    
    if( send_timeout ) nn_setsockopt( socket, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof( send_timeout ) );
    if( recv_timeout ) nn_setsockopt( socket, NN_SOL_SOCKET, NN_RCVTIMEO, &recv_timeout, sizeof( recv_timeout ) );
    return socket;
}