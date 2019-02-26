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

char *decode_err( int err );

typedef struct output_s output;
struct output_s {
    char *socket_str;
    int socket_id;
};

output *output__new() {
    output *self = ( output * ) calloc( 1, sizeof( output ) );
    return self;
}

void output__delete( output *self ) {
    if( self->socket_str ) free( self->socket_str );
}

output *broadcast = NULL;
output *requests  = NULL;
output *results   = NULL;
    
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

void handle_xjr_item( xjr_node *item, char *itemIdStr ) {
    int itemId = atoi( itemIdStr );
    char *type = xjr_node__get_valuez( item, "type", 4 );
    if( !strncmp( type, "cmd", 3 ) ) {
        char *cmd = xjr_node__get_valuez( item, "cmd", 3 );
        
        // Run the cmd, saving the stdout and stderr of it
        // Send a nanomsg request back to the director with the results
        // Use the 'results' socket to send it back
        // Include the itemId in the result message so that it can be correlated
        
        // Example result message:
        // <req type='result' itemId='123' errorCode='0'>
        //   <stdout>...</stdout>
        //   <stderr>...</stderr>
        // </req>
        
        // TODO
        
        free( cmd );
    }
    free( type );
}

void handle_msg( int bytes, char *buf ) {
    printf("Got command: %s\n", buf );
    if( buf[0] == '<' ) {
        xjr_node *root = parse( 0, buf, bytes );
        xjr_node *req = xjr_node__get( root, "req", 3 ); // the request IS the 'item'
        xjr_node *extra = xjr_node__get( root, "extra", 5 );
        char *itemId = xjr_node__get_valuez( extra, "itemId", 6 );
        handle_xjr_item( req, itemId );
        free( itemId );
        xjr_node__delete( root );
    }
    else if( !strncmp( buf, "ping", 4 ) ) {
        printf("Ping; ponging back\n");
        send_data( results, "pong", 4 );
    }
    else if( !strncmp( buf, "reg", 3 ) ) {
        printf("Told to register\n");
        int resSize;
        char *res = request( requests, "reg", 3, &resSize );
        printf("Response of register: %.*s\n", resSize, res );
    }
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

void setup_outputs( xjr_node *outputs ) {
    xjr_node *broadcast_node = xjr_node__get( configRoot, "broadcast"        , 9  );
    xjr_node *requests_node  = xjr_node__get( configRoot, "director_requests", 17 );
    xjr_node *results_node   = xjr_node__get( configRoot, "director_results" , 16 );
    
    broadcast = setup_output( broadcast_node, NN_SUB  );
    requests  = setup_output( requests_node , NN_REQ  );
    results   = setup_output( results_node  , NN_PUSH );
}

void output__cleanup() {
    if( broadcast ) output__delete( broadcast );
    if( requests  ) output__delete( requests  );
    if( results   ) output__delete( results   );
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

void handle_incoming() {
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
        
        handle_msg( bytes, buf );
    }
}

void handle_broadcast() {
    while( running ) {
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
        
        handle_msg( bytes, buf );
    }
}

void *broadcast_thread( void *ptr ) {
    handle_broadcast();
    return NULL;
}

int main( const int argc, const char **argv ) {
    int retCode = 0;
    
    retCode = config();
    pthread_t bThread;
    int bThreadOkay = 1;
    if( !retCode ) {
        signal(SIGINT, intHandler);
        running = 1;
        
        if( pthread_create(&bThread, NULL, broadcast_thread, NULL) ) {
            // broadcast thread failed
            bThreadOkay = 0;
        }
        handle_incoming();
    }
    if( bThreadOkay ) pthread_join( bThread, NULL );
    
    printf("\nShutting down\n");
    output__cleanup();
    if( socket_address_in ) free( socket_address_in );
    if( configRoot ) xjr_node__delete( configRoot );
    return retCode;
}

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
