#include<nanomsg/nn.h>
#include<nanomsg/pipeline.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<xjr-helpers.h>
#include<xjr-node.h>
#include<signal.h>

char *decode_err( int err );

typedef struct output_s output;
struct output_s {
    char *path;
    int path_len;
    char *socket_str;
    int socket_id;
    output *next;        
};

output *output__new() {
    output *self = ( output * ) calloc( 1, sizeof( output ) );
    return self;
}

void output__delete( output *self ) {
    if( self->socket_str ) free( self->socket_str );
}

output *firstOutput = NULL;
output *defaultOutput = NULL;
int output_count = 0;
char *socket_address_in = NULL;
static volatile int running = 0;
int socket_in = 0;
xjr_node *configRoot = NULL;

void handle_msg( int bytes, char *buf ) {
    int socket_dest = defaultOutput->socket_id;
    char *pathpos = strstr( (char *) buf, "path='" );
    if( pathpos ) {
        pathpos += 6;
        output *curOutput = firstOutput;
        for( int j=0;j<output_count;j++ ) {
            if( !curOutput ) break;
            if( !strncmp( pathpos, curOutput->path, curOutput->path_len ) ) {
                socket_dest = curOutput->socket_id;
                break;
            }
            curOutput = curOutput->next;
        }
    }
    
    int sent_bytes = nn_send(socket_dest, buf, bytes, 0 );
    if( !sent_bytes ) {
        int err = errno;
        char *errStr = decode_err( err );
        printf("failed to resend: %s\n",errStr);
        free( errStr );
        nn_freemsg( buf );
    }
}

void setup_outputs( xjr_arr *outputs ) {
    output_count = outputs->count;
    int send_timeout = 200;
    for( int i=0;i<output_count;i++ ) {
        void *itemV = outputs->items[i];
        //char type = outputs->types[i];
        xjr_node *item = (xjr_node *) itemV;
        
        output *oldFirst = firstOutput;
        output *cur = firstOutput = output__new();

        cur->next       = oldFirst;
        cur->path       = xjr_node__get_valuez( item, "path",   4 );
        if( cur->path ) {
            cur->path_len = strlen( cur->path );
        }
        cur->socket_str = xjr_node__get_valuez( item, "socket", 6 );
        if( cur->socket_str ) {
            int socket_id = nn_socket( AF_SP, NN_PUSH );
            int bind_res = nn_bind( socket_id, cur->socket_str );
            if( bind_res < 0 ) {
                fprintf( stderr, "Error binding %s\n", cur->socket_str );
            }
            else { // success
                cur->socket_id = socket_id;
            }   
            nn_setsockopt( socket_id, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof( send_timeout ) );
        }
        xjr_node *defaultFlag = xjr_node__get( item, "default", 7 );
        if( defaultFlag ) {
            defaultOutput = cur;
        }
    }
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
    
    xjr_arr *outputs = xjr_node__getarr( configRoot, "output", 6 );
    if( !outputs ) {
        fprintf( stderr, "Config does not contain any output node\n" );
        return 3;
    }

    socket_address_in = xjr_node__get_valuez( input, "socket", 6 );
    if( !socket_address_in ) {
        fprintf( stderr, "Input node does not have a socket\n" );
        return 4;
    }
    
    umask( 0 );
    int res = setup_input();
    if( res ) return res;
    setup_outputs( outputs );
    
    return 0;
}

void intHandler(int dummy) {
    running = 0;
}

int main( const int argc, const char **argv ) {
    int retCode = 0;
    
    int res = config();
    if( res ) {
        retCode = res;
        goto cleanup;
    }
    
    signal(SIGINT, intHandler);
    running = 1;
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
    
    cleanup:
    printf("\nShutting down\n");
    if( firstOutput ) {
        output *curOutput = firstOutput;
        for( int j=0;j<output_count;j++ ) {
            output *nextOutput = curOutput->next;
            output__delete( curOutput );
            curOutput = nextOutput;
        }
    }
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
