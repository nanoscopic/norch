#include"broadcast_handler.h"
#include<stdio.h>
#include<stdlib.h>
#include"misc.h"
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include<pthread.h>

pthread_t broadcast_thread;
int broadcast_started = 0;
volatile int *broadcast_running;

void *broadcast_thread_func( void *ptr ) {
    broadcast_loop();
    return NULL;
}

void join_broadcast_handler() {
    if( broadcast_started ) pthread_join( broadcast_thread, NULL );
}

void spawn_broadcast_handler( volatile int *running ) {
    broadcast_running = running;

    if( pthread_create( &broadcast_thread, NULL, broadcast_thread_func, NULL ) ) {
        // thread start failed
    }
    else {
        broadcast_started = 1;
    }
}

void broadcast_handle_incoming_bytes( int bytes, char *buf ) {
    // TODO
}

void broadcast_loop() {
    // TODO pass in broadcast socket
    /*while( running ) {
        void *buf = NULL;
        int bytes = nn_recv(broadcast->socket_id, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           if( err == ETIMEDOUT ) {
               printf("x");
               fflush(stdout);
               continue;
           }
           if( err == EINTR ) {
               printf("Got ctrl-c in broadcast thread\n");
               running = 0;
               break;
           }
           char *errStr = decode_err( err );
           printf( "failed to receive: %s\n",errStr);
           free( errStr );
           continue;
        }
        
        printf("Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        broadcast_handle_incoming_bytes( bytes, buf );
    }*/
}