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
    
    if( btype == bind ) {
        fprintf(stderr,"Binding to %s\n", address );
        bind_res = nn_bind( socket, address );
    }
    else if( btype == connect ) {
        fprintf(stderr,"Connecting to %s\n", address );
        bind_res = nn_connect( socket, address );
    }
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

split_line *split_line__new() {
    split_line *self = (split_line *) calloc( sizeof( split_line ), 1 );
    self->next = NULL;
    return self;
}

// Split up text into NULL delimited line strings ( lacking the \n's )
split_line *split_lines( char *data, long int dataLen, int *lcountP ) {
    split_line *curLine = split_line__new();
    split_line *firstLine = curLine;
    long int i;
    int lCount = 1;
    int lStart = 0;
    split_line *prevLine = curLine;
    for( i=0;i<dataLen;i++ ) {
        char let = data[ i ];
        if( let == '\n' ) {
            int len = i - lStart + 1;
            curLine->line = malloc( len + 1 );
            curLine->line[ len - 1 ] = 0x00;
            memcpy( curLine->line, data + lStart, len - 1 );
            //printf("%i:%s\n",lCount,curLine->line);
            curLine->next = split_line__new();
            prevLine = curLine;
            curLine = curLine->next;
            lCount++;
            lStart = i + 1;
        }
    }
    if( data[ i-1 ] == '\n' ) {
        //free( curLine );
        prevLine->next = NULL;
        lCount--;
    }
    else {
        int len = i - lStart + 1;
        curLine->line = malloc( len + 1 );
        curLine->line[ len + 1 ] = 0x00;
        memcpy( curLine->line, data + lStart, len );
    }
    if( lcountP ) *lcountP = lCount;
    return firstLine;
}