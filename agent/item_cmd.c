#include "item_cmd.h"
#include<xjr-node.h>
#include<xjr-helpers.h>
#include <unistd.h>
#include <fcntl.h>
#include<string.h>
#include <sys/wait.h>

cmd_res *run_cmd( char **args, int argLen, long int *inLen, char *in ) {
    // Run the cmd, saving the stdout and stderr of it
    int stdout_pipe[2];
    if( pipe( stdout_pipe ) == -1 ) return NULL;
    int stderr_pipe[2];
    if( pipe( stderr_pipe ) == -1 ) return NULL;
    
    pid_t pid;
    if( ( pid = fork() ) == -1 ) return NULL;
    
    if(pid == 0) {
        dup2( stdout_pipe[1], STDOUT_FILENO ); close( stdout_pipe[0] ); close( stdout_pipe[1] );
        dup2( stderr_pipe[1], STDERR_FILENO ); close( stderr_pipe[0] ); close( stderr_pipe[1] );
        
        execv( args[0], args );
        
        exit(1); // Should never be reached
    }
    
    close( stdout_pipe[1] );
    close( stderr_pipe[1] );
    
    fcntl( stdout_pipe[0], F_SETFL, O_NONBLOCK | O_ASYNC);
    fcntl( stderr_pipe[0], F_SETFL, O_NONBLOCK | O_ASYNC);   
    
    int outSize = 100;
    int errSize = 100;
    int outPos = 0;
    int errPos = 0;
    char *out = malloc( outSize );
    char *err = malloc( errSize );
        
    int resCode = 1000;
    int done = 0;
    while( done != 2 ) {
        // read a chunk of stdout
        int out_bytes = read( stdout_pipe[0], ( out + outPos ), outSize - outPos );
        if( out_bytes > 0 ) {
            outPos += out_bytes;
            if( outPos == outSize ) {
                char *newbuf = malloc( outSize * 2 );
                memcpy( newbuf, out, outSize );
                free( out );
                out = newbuf;
                outSize *= 2;
            }
        }
        
        // read a chunk of stderr
        int err_bytes = read( stderr_pipe[0], ( err + errPos ), errSize - errPos );
        //printf("Error bytes: %i\n", err_bytes );
        if( err_bytes > 0 ) {
            errPos += err_bytes;
            if( errPos == errSize ) {
                char *newbuf = malloc( errSize * 2 );
                memcpy( newbuf, err, errSize );
                free( err );
                err = newbuf;
                errSize *= 2;
                //printf("New size: %i\n", errSize );
            }
        }
        
        if( out_bytes < 1 && err_bytes < 1 && done == 1 ) done = 2;
        
        if( !done ) {
            int status = 1111;
            int pid = waitpid( 0, &status, WNOHANG ); 
            
            if( pid != 0 ) {
                if( WIFEXITED( status ) ) {
                    resCode = WEXITSTATUS( status );
                    done = 1;
                }
                else if( WIFSIGNALED( status ) ) {
                    int signal = WTERMSIG( status );
                    // resCode is 1111 because we haven't changed it
                    resCode = signal;
                    done = 1;
                }
            }
        }
        
        // sleep for 1/100 of a second
        if( out_bytes < 1 && err_bytes < 1 ) usleep( 10000 );
    }
    
    cmd_res *res = calloc( sizeof( cmd_res ), 1 );
    res->outLen = outPos;
    res->errLen = errPos;
    res->out = out;
    res->err = err;
    res->errorLevel = resCode;
    return res;
}

void cmd_res__delete( cmd_res *self ) {
}

int xjr_to_args( xjr_arr *argArr, int *argCount, char ***argsOut ) {
    char **args;
    if( !argArr ) return 1;
    int count = argArr->count;
    printf("Number of arguments: %i\n", count );
    args = malloc( sizeof( void * ) * ( count + 2 ) );
    args[ argArr->count+1 ] = NULL;
    
    for( int i=0;i<count;i++ ) {
        xjr_node *arg = ( xjr_node * ) argArr->items[ i ];
        int vallen;
        char *value = xjr_node__value( arg, &vallen );
        char *valuez = malloc( vallen + 1 );
        valuez[ vallen ] = 0x00;//NULL;
        memcpy( valuez, value, vallen );
        args[ i+1 ] = valuez;
        printf("Argument %i: '%s'\n", i, valuez );
    }
    xjr_arr__delete( argArr );
    
    *argsOut = args;
    return 0;
}

void free_args( char **args, int argLen ) {
    for( int i=1;i<argLen;i++ ) {
        char *ptr = args[ i ];
        free( ptr );
    }
    free( args );
}

char *form_response( int itemId, cmd_res *res ) {
    int len = 20 + 26 + 44 + 44 + 21 // Size of template ( see below )
                  + res->outLen // size of stdout
                  + res->errLen // size of stderr
                  + 20 // some extra to handle integer expansion
                  ;
    char *msg = malloc( len );
    snprintf( msg, len, 
        "<result itemId='%i'>" // 20
        "<localvars errorCode='%i'>" // 26
        "<stdout bytes='%li'><![CDATA[%.*s]]></stdout>" // 44
        "<stderr bytes='%li'><![CDATA[%.*s]]></stderr>" // 44
        "</localvars></result>", // 21
        itemId,
        res->errorLevel,
        res->outLen,
        (int) res->outLen,
        res->out,
        res->errLen,
        (int) res->errLen,
        res->err );
    return msg;
}

char *item_cmd( xjr_node *item, char *itemIdStr ) {
    char *cmd = xjr_node__get_valuez( item, "cmd", 3 );
    
    int itemId = atoi( itemIdStr );
    free( itemIdStr );
    
    // Ensure the specified file exists and can be executed
    if( access( cmd, X_OK ) == -1 ) {
        char err[400];
        sprintf(err,"error:File does not exist or is not executable '%s'", cmd );
        return strdup( err );
    }
    
    int argCount = 0;
    char **args;
    int argRes = xjr_to_args( xjr_node__getarr( item, "arg", 3 ), &argCount, &args );
    if( argRes ) {
        xjr_node__dump( item, 20 );
        return strdup("error:no argument specified");
    }
    args[ 0 ] = cmd;
    
    char *in = NULL;
    cmd_res *runRes = run_cmd( args, argCount, 0, in );
    if( !runRes ) {
        char *msg = form_response( itemId, runRes );
        cmd_res__delete( runRes );
        printf( msg );
        free_args( args, argCount );
        return msg;
    }

    return strdup("error:unknown");
}