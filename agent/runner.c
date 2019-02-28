#include "runner.h"
#include "returner.h"
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<xjr-machine.h>
#include<nanomsg/nn.h>
#include<nanomsg/pubsub.h>
#include<nanomsg/pipeline.h>
#include<nanomsg/reqrep.h>
#include<pthread.h>
#include"misc.h"
#include"item_cmd.h"
#include<string.h>
#include"output.h"

pthread_t runner_thread;
int runner_started = 0;
volatile int *runner_running;
int runner_ipc_socket;
char *runner_address = "inproc://runner";

void runner_setup_ipc() {
    int send_timeout = 200;
    int recv_timeout = 200; // Note that this recv timeout does not make send for request reply as those can take longer
    
    int socket = nn_socket( AF_SP, NN_PULL );
    int bind_res = nn_bind( socket, runner_address );
    if( bind_res < 0 ) {
        fprintf( stderr, "Error binding %s\n", runner_address );
        return;
    }
    
    nn_setsockopt( socket, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof( send_timeout ) );
    nn_setsockopt( socket, NN_SOL_SOCKET, NN_RCVTIMEO, &recv_timeout, sizeof( recv_timeout ) );
    
    runner_ipc_socket = socket;
}

void runner_ipc_close() {
    // TODO close ipc socket
}

void *runner_thread_func( void *ptr ) {
    runner_loop();
    return NULL;
}

void runner_cleanup() {
    runner_ipc_close();
}

void runner_loop() {
    while( *runner_running ) {
        run_some_items();
        
        void *buf = NULL;
        int bytes = nn_recv(runner_ipc_socket, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           if( err == ETIMEDOUT ) {
               printf("x");
               fflush(stdout);
               continue;
           }
           if( err == EINTR ) {
               printf("Got ctrl-c in runner thread\n");
               //*runner_running = 0;
               break;
           }
           char *errStr = decode_err( err );
           printf( "failed to receive: %s\n",errStr);
           free( errStr );
           continue;
        }
        
        printf("Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        runner_queue_incoming_bytes( bytes, buf );
    }
    
    runner_cleanup();
}

void spawn_runner( volatile int *running ) {
    runner_running = running;

    if( pthread_create( &runner_thread, NULL, runner_thread_func, NULL ) ) {
        // thread start failed
    }
    else {
        runner_started = 1;
    }
}

void join_runner() {
    if( runner_started ) pthread_join( runner_thread, NULL );
}

runnerItem *firstRunnerItem;

runnerItem *runnerItem__add( char *data, int len ) {
    runnerItem *self = ( runnerItem * ) calloc( 1, sizeof( runnerItem ) );
    return self;
}

void runnerItem__delete( output *self ) {
    free( self );
}

runnerItem *runnerItem__pop() {
    if( !firstRunnerItem ) return NULL;
    runnerItem *ret = firstRunnerItem;
    firstRunnerItem = ret->next;
    return ret;
}

void runner_queue_incoming_bytes( int bytes, char *buf ) {
    runnerItem__add( buf, bytes );
}

void run_some_items() {
    runnerItem *item = runnerItem__pop();
    if( !item ) return;
    runner_handle_runnerItem( item );
}

void runner_handle_runnerItem( runnerItem *item ) {
    int bytes = item->len;
    char *buf = item->data;
    
    if( buf[0] == '<' ) {
        printf("Got xjr commandset\n");
        xjr_node *root = parse( 0, buf, bytes );
        
        xjr_node *extra = xjr_node__get( root, "extra", 5 );
        char *itemId = xjr_node__get_valuez( extra, "itemId", 6 );
        
        xjr_node *curItem = xjr_node__firstChild( root );
        while( curItem ) {
            // TODO: queue the result for execution rather than blocking incoming thread
            runner_handle_item( curItem, itemId );
            curItem = curItem->next;
        }
        
        free( itemId );
        
        xjr_node__delete( root );
        return;
    }
    
    printf("Got raw command: %s\n", buf );
    if( !strncmp( buf, "ping", 4 ) ) {
        //printf("Ping; ponging back\n");
        //send_data( results, "pong", 4 );
    }
    else if( !strncmp( buf, "reg", 3 ) ) {
        /*printf("Told to register\n");
        int resSize;
        char *res = request( requests, "reg", 3, &resSize );
        printf("Response of register: %.*s\n", resSize, res );*/
    }
}

void runner_handle_item( xjr_node *item, char *itemIdStr ) {
    //int itemId = atoi( itemIdStr );
    // TODO something with itemID
    
    //char *type = xjr_node__get_valuez( item, "type", 4 );
    
    // There is no namez function :(
    int typeLen;
    char *type = xjr_node__name( item, &typeLen );
    char *typez = malloc( typeLen + 1 );
    memcpy( typez, type, typeLen );
    typez[ typeLen ] = 0x00;
    
    char *result = NULL;
    if     ( !strncmp( type, "cmd", 3 ) ) { result = item_cmd( item, itemIdStr ); }
    //else if( !strncmp( type, ...      ) ) { result = item_...( item, itemIdStr ); }
    
    if( result ) {
        // TODO: queue the result for sending back to the director
        // we cannot directly enqueue result; we need to send it via nanomsg to the returner thread
        //returner_queue_result( result );
        
        //free( result );
    }
    
    free( typez );
}