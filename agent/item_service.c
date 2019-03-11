#include "item_service.h"
#include<xjr-node.h>
#include<xjr-helpers.h>
#include "item_cmd.h"
#include<string.h>
#include<stdlib.h>
#include"subreg.h"
#include <stdarg.h>
#include"misc.h"
#include"line_matcher.h"

char **gen_args( int count, ... ) {
    char **arr = calloc( sizeof( char * ) * ( count + 1 ), 1 );
    arr[ count ] = NULL;
    
    va_list args;
    va_start(args,count);
    for( int i=0;i<count;i++ ) {
        char *arg = va_arg(args, char *);
        arr[ i ] = arg;
    }
    va_end(args);
    return arr;
}

split_line *systemctl_lines( char *cmd, char *unit, int *errLevelP ) {
    char **args = gen_args( 3, "/usr/bin/systemctl", cmd, unit );
    int argCount = 4;
    char *in = NULL;
    cmd_res *res = run_cmd( args, argCount, 0, in );
    free(args);
    if( errLevelP ) *errLevelP = res->errorLevel;
    if( res->out ) {
        //printf("\nOutput:\n%.*s\n", (int) res->outLen, res->out );
        return split_lines( res->out, res->outLen, 0 );
    }
    return NULL;
}

char *service_start( xjr_node *item, char *unit, int itemId ) {
    char **args = gen_args( 3, "/usr/bin/systemctl", "start", unit );
    int argCount = 4;
    char *in = NULL;
    cmd_res *res = run_cmd( args, argCount, 0, in );
    free(args);
    if( res->errorLevel == 0 ) return strdup("<result><localvars success='1' /></results>");
    else                       return strdup("<result><localvars success='0' /></results>");
}

char *service_status( xjr_node *item, char *unit, int itemId ) {
    int errLevel;
    split_line *curLine = systemctl_lines( "status", unit, &errLevel );
    if( errLevel == 4 ) {
        return strdup( "<result><localvars success='0' text='Service does not exist'/></result>" );
    }
    if( curLine ) {
        line_matcher *matcher = line_matcher__new();
        line_matcher__add_exp( matcher, "\\s+Active: (\\S+).+", 1 );
        matched_line *curMatch = line_matcher__match( matcher, curLine );
        while( curMatch ) {
            int pNum = curMatch->patternNum;
            if( pNum == 1 ) {
                subreg_capture_t *line = curMatch->captures;
                printf("Matched line: %s\n", line->start );
            }
            curMatch = curMatch->next;
        }
    }
    return strdup( "<result><localvars success='1'/></result>");
}

char *service_stop( xjr_node *item, char *unit, int itemId ) {
    char **args = gen_args( 3, "/usr/bin/systemctl", "stop", unit );
    int argCount = 4;
    char *in = NULL;
    cmd_res *res = run_cmd( args, argCount, 0, in );
    free(args);
    if( res->errorLevel == 0 ) return strdup("<result><localvars success='1' /></results>");
    else                       return strdup("<result><localvars success='0' /></results>");
}

char *service_enable( xjr_node *item, char *unit, int itemId ) {
    char **args = gen_args( 3, "/usr/bin/systemctl", "enable", unit );
    int argCount = 4;
    char *in = NULL;
    cmd_res *res = run_cmd( args, argCount, 0, in );
    free(args);
    if( res->errorLevel == 0 ) return strdup("<result><localvars success='1' /></results>");
    else                       return strdup("<result><localvars success='0' /></results>");
}

char *service_disable( xjr_node *item, char *unit, int itemId ) {
    char **args = gen_args( 3, "/usr/bin/systemctl", "disable", unit );
    int argCount = 4;
    char *in = NULL;
    cmd_res *res = run_cmd( args, argCount, 0, in );
    free(args);
    if( res->errorLevel == 0 ) return strdup("<result><localvars success='1' /></results>");
    else                       return strdup("<result><localvars success='0' /></results>");
}

char *service_install( xjr_node *item, char *unit, int itemId ) {
    // TODO
    // Copy the service file to the appropriate location
    return NULL;
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
         if( !strncmp( action, "stop",    4 ) ) return service_stop(    item, unit, itemId );
    // Ensure a service is running
    else if( !strncmp( action, "start",   5 ) ) return service_start(   item, unit, itemId );
    // Make sure a service is enabled
    else if( !strncmp( action, "enable",  6 ) ) return service_enable(  item, unit, itemId );
    // Make sure a service is disabled
    else if( !strncmp( action, "disable", 7 ) ) return service_disable( item, unit, itemId );  
    // Install a service unit file and/or update existing unit file
    else if( !strncmp( action, "install", 7 ) ) return service_install( item, unit, itemId );
    // Check status of a service
    else if( !strncmp( action, "status", 6  ) ) return service_status(   item, unit, itemId );
    
    responseLen = 200;
    msg = malloc( responseLen );
    snprintf( msg, responseLen, "<result itemId='%i'><error>unkown action</error></localvars></result>", itemId );

    return msg;
}