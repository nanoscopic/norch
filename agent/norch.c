/*
 * Original Copyright (C) David Helkowski 2017, 2018
 * Original License: AGPL
 * Forking MIT with permission to allow usage and derivative work by SUSE LLC
 *
 * Ongoing Copyright (C) SUSE LLC 2019
 */
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<signal.h>
#include<pthread.h>
#include<unistd.h>
#include "output.h"
#include "misc.h"
#include "broadcast_handler.h"
#include "returner.h"
#include "runner.h"

char *decode_err( int err );

output *broadcast = NULL;
output *requests  = NULL;
    
char *socket_address_in = NULL;
static volatile int running = 0;
int socket_in = 0;
xjr_node *configRoot = NULL;

int send_data( output *dest, char *data, int dataLen ) {
    int sent_bytes = nn_send( dest->socket_id, data, dataLen, 0 );
    if( !sent_bytes ) {
        int err = errno;
        char *errStr = decode_err( err );
        printf("failed to resend: %s\n",errStr);
        free( errStr );
        //nn_freemsg( buf );
        return 1;
    }
    return 0;
}

char *request( output *dest, char *data, int dataLen, int *respSize ) {
    int res = send_data( dest, data, dataLen );
    if( res ) return NULL;
    char *response = malloc( 2000 );
    int recv_bytes = nn_recv( dest->socket_id, response, 2000, 0);
    *respSize = recv_bytes;
    return response;
}

void setup_outputs( xjr_node *outputs ) {
    xjr_node *broadcast_node = xjr_node__get( configRoot, "broadcast"        , 9  );
    xjr_node *requests_node  = xjr_node__get( configRoot, "director_requests", 17 );
    
    broadcast = setup_output( broadcast_node, NN_SUB  );
    requests  = setup_output( requests_node , NN_REQ  );
    
    returner_setup_output( configRoot );
}

void cleanup_outputs() {
    if( broadcast ) output__delete( broadcast );
    if( requests  ) output__delete( requests  );
    
    // TODO returner_cleanup_output();
}

int setup_input() {
    int recv_timeout = 200;
    
    socket_in = nn_socket(AF_SP, NN_PULL);
    
    int bind_res = nn_bind(socket_in, socket_address_in);
    if( bind_res < 0 ) {
        fprintf( stderr, "Bind to %s error\n", socket_address_in);
        return 5;
    }
    nn_setsockopt( socket_in, NN_SOL_SOCKET, NN_RCVTIMEO, &recv_timeout, sizeof( recv_timeout ) );
    
    return 0;
}

int config() {
    char *configFile = "config.xjr";
    char *buffer;
    
    configRoot = parse_file( (xjr_mempool *) 0, configFile, &buffer );
    if( !configRoot ) {
        fprintf( stderr, "Could not open config file %s\n", configFile );
        return 1;
    }
    xjr_node *input = xjr_node__get( configRoot, "input", 5 );
    if( !input ) {
        fprintf( stderr, "Config does not contain input node\n" );
        return 2;
    }
    
    socket_address_in = xjr_node__get_valuez( input, "socket", 6 );
    if( !socket_address_in ) {
        fprintf( stderr, "Input node does not have a socket\n" );
        return 4;
    }
    
    umask( 0 );
    int res = setup_input();
    if( res ) return res;
    setup_outputs( configRoot );
    
    return 0;
}

void intHandler(int dummy) {
    running = 0;
}

void incoming_loop() {
    while( running ) {
        void *buf = NULL;
        int bytes = nn_recv(socket_in, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           if( err == ETIMEDOUT ) {
               printf(".");
               fflush(stdout);
               continue;
           }
           if( err == EINTR ) {
               printf("Got ctrl-c in main thread\n");
               running = 0;
               break;
           }
           char *errStr = decode_err( err );
           printf( "failed to receive: %s\n",errStr);
           free( errStr );
           continue;
        }
        
        printf("Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        // TODO properly send message to runner; cannot directly enqueue
        //runner_queue_incoming_bytes( bytes, buf );
    }
}

int main( const int argc, const char **argv ) {
    int configError = config();
    
    if( !configError ) {
        signal(SIGINT, intHandler);
        running = 1;
        
        spawn_runner( &running );
        spawn_returner( &running );
        spawn_broadcast_handler( &running );
        
        incoming_loop();
    }
    
    join_runner();
    join_returner();
    join_broadcast_handler();
    
    printf("\nShutting down\n");
    cleanup_outputs();
    if( socket_address_in ) free( socket_address_in );
    if( configRoot ) xjr_node__delete( configRoot );
    return configError;
}