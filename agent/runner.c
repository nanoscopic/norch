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
char *runner_address = "tcp://127.0.0.1:8787";//"inproc://runner";
int runner_gate_to_returner;

void runner_setup_ipc() {
    runner_ipc_socket = make_nn_socket( runner_address, bind, NN_PULL, 0, 200 );
}

int runner_open_gate() {
    return make_nn_socket( runner_address, connect, NN_PUSH, 300, 0 );
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
    runner_gate_to_returner = returner_open_gate();
    
    while( *runner_running ) {
        run_some_items();
        
        void *buf = NULL;
        int bytes = nn_recv(runner_ipc_socket, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           if( err == ETIMEDOUT ) {
               printf("RN ");
               fflush(stdout);
               continue;
           }
           if( err == EINTR ) {
               printf("Got ctrl-c in runner thread\n");
               //*runner_running = 0;
               break;
           }
           char *errStr = decode_err( err );
           printf( "Runner failed to receive: %s\n",errStr);
           free( errStr );
           continue;
        }
        
        printf("\nRunner Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        runner_queue_incoming_bytes( bytes, buf );
    }
    
    runner_cleanup();
    // TODO close runner_gate_to_returner
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
    self->data = data;
    self->len = len;
    if( firstRunnerItem ) {
        self->next = firstRunnerItem;
    }
    firstRunnerItem = self;
    return self;
}

void runnerItem__delete( output *self ) {
    free( self );
}

runnerItem *runnerItem__pop() {
    if( !firstRunnerItem ) return NULL;
    printf("\nRunner - got item off queue\n");
    runnerItem *ret = firstRunnerItem;
    firstRunnerItem = ret->next;
    return ret;
}

void runner_gated_incoming_bytes( int gate, int bytes, char *buf ) {
    fprintf(stderr,"\nTrying to send %i bytes into runner\n", bytes );
    
    for( int i=0;i<4;i++ ) {
        int sent_bytes = nn_send( gate, buf, bytes, 0 ); 
        if( sent_bytes == -1 ) {
            int err = errno;
            char *errStr = decode_err( err );
            printf( "\n  failed to send: %s\n",errStr);
            free( errStr );
        }
        else {
            printf("  success\n");
            break;
        }
    }
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
    else {
        fprintf(stderr,"Running received command of type %s", typez );
    }
    
    if( result ) {
        // TODO: queue the result for sending back to the director
        // we cannot directly enqueue result; we need to send it via nanomsg to the returner thread
        int resLen = strlen( result );
        printf("\nitem cmd returned %i bytes\n", resLen );
        
        returner_gated_incoming_bytes( runner_gate_to_returner, resLen, result );
        
        free( result );
    }
    
    free( typez );
}