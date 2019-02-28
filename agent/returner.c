// Queues and returns result to the director
#include "returner.h"
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include "output.h"
#include<pthread.h>
#include"misc.h"

output *results_socket = NULL;

pthread_t returner_thread;
int returner_started = 0;
volatile int *returner_running;
int returner_ipc_socket;
char *returner_address = "inproc://returner";

void returner_setup_ipc() {
    int send_timeout = 200;
    int recv_timeout = 200; // Note that this recv timeout does not make send for request reply as those can take longer
    
    int socket = nn_socket( AF_SP, NN_PULL );
    int bind_res = nn_bind( socket, returner_address );
    if( bind_res < 0 ) {
        fprintf( stderr, "Error binding %s\n", returner_address );
        return;
    }
    
    nn_setsockopt( socket, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof( send_timeout ) );
    nn_setsockopt( socket, NN_SOL_SOCKET, NN_RCVTIMEO, &recv_timeout, sizeof( recv_timeout ) );
    
    returner_ipc_socket = socket;
}

void returner_ipc_close() {
    // TODO close ipc socket
}

void *returner_thread_func( void *ptr ) {
    returner_loop();
    return NULL;
}

void returner_queue_incoming_bytes( int bytes, char *buf ) {
    returnerItem__add( buf, bytes );
}

void return_some_items() {
    returnerItem *item = returnerItem__pop();
    if( !item ) return;
    returner_handle_item( item );
}

void returner_handle_item( returnerItem *item ) {
    char *data = item->data;
    int len = item->len;
    
    printf("Sending results '%.*s'\n", len, data );
    int sentBytes = nn_send( results_socket->socket_id, data, len, 0 );
    if( sentBytes == -1 ) {
        // TODO error
    }
}

void returner_loop() {
    while( *returner_running ) {
        return_some_items();
        
        void *buf = NULL;
        int bytes = nn_recv(returner_ipc_socket, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           if( err == ETIMEDOUT ) {
               printf("x");
               fflush(stdout);
               continue;
           }
           if( err == EINTR ) {
               printf("Got ctrl-c in returner thread\n");
               //running = 0;
               break;
           }
           char *errStr = decode_err( err );
           printf( "failed to receive: %s\n",errStr);
           free( errStr );
           continue;
        }
        
        printf("Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        returner_queue_incoming_bytes( bytes, buf );
    }
}

void returner_setup_output( xjr_node *configRoot ) {
    xjr_node *results_node = xjr_node__get( configRoot, "director_results" , 16 );
    results_socket = setup_output( results_node  , NN_PUSH );
}

void returner_cleanup_output() {
    if( results_socket ) output__delete( results_socket );
}

void returner_queue_result( char *result ) {
}

void returner_send_result() {
}

void spawn_returner( volatile int *running ) {
    returner_running = running;

    if( pthread_create( &returner_thread, NULL, returner_thread_func, NULL ) ) {
        // thread start failed
    }
    else {
        returner_started = 1;
    }
}

void join_returner() {
    if( returner_started ) pthread_join( returner_thread, NULL );
}

returnerItem *firstReturnerItem;

returnerItem *returnerItem__add( char *data, int len ) {
    returnerItem *self = ( returnerItem * ) calloc( 1, sizeof( returnerItem ) );
    return self;
}

void returnerItem__delete( returnerItem *self ) {
    free( self );
}

returnerItem *returnerItem__pop() {
    if( !firstReturnerItem ) return NULL;
    returnerItem *ret = firstReturnerItem;
    firstReturnerItem = ret->next;
    return ret;
}