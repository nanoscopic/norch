#include "item_service.h"
#include<xjr-node.h>
#include<xjr-helpers.h>
#include "item_cmd.h"
#include<string.h>
#include<stdlib.h>
#include"subreg.h"

// Split text into up to 50 lines. Text containing more than 50 lines will only return the 
// offsets of the first 50 lines.
int split_lines( char *data, long int dataLen, char ***lposP, long int **lsizeP ) {
    //long int *lpos = calloc( sizeof( long int ) * 50, 1 );
    char **lpos = calloc( sizeof( char * ) * 50, 1 );
    long int *lsize = calloc( sizeof( long int ) * 50, 1 );
    int lfn = 0; // line feed number
    long int i;
    int lCount = 1;
    int lStart = 0;
    lpos[0] = data;
    for( i=0;i<dataLen;i++ ) {
        char let = data[ i ];
        if( let == '\n' ) {
            lsize[ lfn ] = i - lStart + 1;
            lpos[ ++lfn ] = data + i + 1;
            lCount++;
            lStart = i + 1;
        }
        if( lfn == 50 ) break;
    }
    // TODO: Store the very last line
    *lposP = lpos;
    *lsizeP = lsize;
    return lCount;
}

int systemctl_lines( char *cmd, char *unit, int *lineCountP, char ***lposP, long int **lsizeP ) {
    // systemctl start [unit]
    char **args = malloc( sizeof( char * ) * 4 );
    args[0] = "/usr/bin/systemctl";
    args[1] = cmd;//"status";
    args[2] = unit;//"firewalld";
    args[3] = NULL;
    int argCount = 4;
    
    char *out = NULL;
    long int outLen;
    char *err;
    long int errLen;
    char *in = NULL;
    int errLevel;
    int runRes = run_cmd( args, argCount, &outLen, &out, &errLen, &err, 0, in, &errLevel );
    free(args);
    printf("\n");
    if( out ) {
        printf("Output:\n%.*s\n", (int) outLen, out );
        char **lpos = NULL;
        long int *lsize = NULL;
        int lCount = split_lines( out, outLen, &lpos, &lsize );
        *lposP = lpos;
        *lsizeP = lsize;
        *lineCountP = lCount;
        printf("lCount: %i\n", lCount );
        
        return 0;
    }
    return 1;
}

char *service_start( xjr_node *item, int itemId ) {
    int lCount;
    char **lpos = NULL;
    long int *lsize = NULL;
    int err = systemctl_lines( "status", "firewalld", &lCount, &lpos, &lsize );
    if( !err ) {
        for( int i=0; i<lCount; i++ ) {
            char *line = lpos[ i ];
            int lLen = lsize[ i ];
            //printf("%i:%.*s\n", i, (int) lLen - 1, line );
            // TODO: Check the line to see if it matches a regex we care about. If so process the line.
            char *zline = strndup( line, lLen - 1 );
            printf("%i:%s\n", i, zline );
            subreg_capture_t captures[2];
            int mres = subreg_match( "\\s+Active: (\\S+).+", zline, &captures, 2,4 );
            if( mres > 0 ) {
                int len1 = captures[1].length;
                printf("Matches %.*s\n", len1, captures[1].start );
            }
            else {
                printf("Res: %i\n", mres );
            }
            free( zline );
        }
        if( lpos ) free( lpos );
        if( lsize ) free( lsize );
    }
    return "<result/>";
}

char *service_stop( xjr_node *item, int itemId ) {
    // systemctl stop [unit]
}

char *service_enable( xjr_node *item, int itemId ) {
    // systemctl enable [unit]
}

char *service_disable( xjr_node *item, int itemId ) {
    // systemctl disable [unit]
}

char *service_install( xjr_node *item, int itemId ) {
    // TODO
    // Copy the service file to the appropriate location
}

char *item_service( xjr_node *item, char *itemIdStr ) {
    int responseLen;
    
    int itemId = atoi( itemIdStr );
    free( itemIdStr );
    
    char *msg;
    
    char *unit   = xjr_node__get_valuez( item, "unit"  , 4 );
    if( !unit ) {
        responseLen = 200;
        msg = malloc( responseLen );
        snprintf( msg, responseLen, "<result itemId='%i'><error>unit not specified</error></localvars></result>", itemId );
    }
    char *action = xjr_node__get_valuez( item, "action", 6 );
    if( !action ) {
        responseLen = 200;
        msg = malloc( responseLen );
        snprintf( msg, responseLen, "<result itemId='%i'><error>action not specified</error></localvars></result>", itemId );
    }
    
    // Ensure a service is not running
         if( !strncmp( action, "stop",    4 ) ) return service_stop(    item, itemId );
    // Ensure a service is running
    else if( !strncmp( action, "start",   5 ) ) return service_start(   item, itemId );
    // Make sure a service is enabled
    else if( !strncmp( action, "enable",  6 ) ) return service_enable(  item, itemId );
    // Make sure a service is disabled
    else if( !strncmp( action, "disable", 7 ) ) return service_disable( item, itemId );  
    // Install a service unit file and/or update existing unit file
    else if( !strncmp( action, "install", 7 ) ) return service_install( item, itemId );
    
    responseLen = 200;
    msg = malloc( responseLen );
    snprintf( msg, responseLen, "<result itemId='%i'><error>unkown action</error></localvars></result>", itemId );

    return msg;
}