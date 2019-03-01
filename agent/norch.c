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
#include "config.h"

char *decode_err( int err );

int broadcast_socket = 0;
int requests_socket  = 0;
int results_socket   = 0;
    
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

void setup_sockets( config *conf ) {
    broadcast_socket = make_nn_socket( conf->director_broadcast_socket, connect, NN_SUB,  1000, 1000 );
    requests_socket  = make_nn_socket( conf->director_requests_socket , connect, NN_REQ,  1000, 1000 );
    results_socket   = make_nn_socket( conf->director_results_socket  , connect, NN_PUSH, 2000, 0 );
    socket_in        = make_nn_socket( conf->input_socket             , bind   , NN_PULL, 200, 0 );
}

void cleanup_outputs() {
    if( broadcast_socket ) nn_close( broadcast_socket );
    if( requests_socket  ) nn_close( requests_socket  );
    if( results_socket   ) nn_close( results_socket   );
}

int doconfig() {
    char *configFile = "config.xjr";
    char *buffer;
    xjr_node__disable_mempool();
    configRoot = parse_file( (xjr_mempool *) 0, configFile, &buffer );
    if( !configRoot ) {
        fprintf( stderr, "Could not open config file %s\n", configFile );
        return 1;
    }
    config *conf = read_config( configRoot );
    print_config( conf );
    
    umask( 0 );
    
    setup_sockets( conf );
    
    // TODO free config root
    
    return 0;
}

void intHandler(int dummy) {
    running = 0;
}

void incoming_loop() {
    int gate_to_runner = runner_open_gate();
    if( gate_to_runner < 0 ) {
        fprintf(stderr,"\nCould not connect to runner\n");
        exit(1);
    }
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
        
        printf("\nIncoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        // TODO properly send message to runner; cannot directly enqueue
        runner_gated_incoming_bytes( gate_to_runner, bytes, buf );
        //runner_queue_incoming_bytes( bytes, buf );
    }
    // TODO close runner gate
}

int main( const int argc, const char **argv ) {
    int configError = doconfig();
    
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