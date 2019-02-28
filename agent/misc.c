#include<stdio.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include<stdlib.h>
#include<string.h>

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