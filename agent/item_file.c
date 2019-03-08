#include "item_file.h"
#include<xjr-node.h>
#include<xjr-helpers.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>

char *item_file( xjr_node *item, char *itemIdStr ) {
    char *path = xjr_node__get_valuez( item, "path", 4 );
    
    //printf("\nFile path: %s\n", path );
    
    int itemId = atoi( itemIdStr );
    free( itemIdStr );
    
    xjr_node *dataNode = xjr_node__get( item, "data", 4 );
    char *data = dataNode->val;
    int dataLen = dataNode->vallen;
    
    int accessRes = access( path, F_OK );
    
    FILE *fh;
    
    char *res = "ok";
    char *oldData = NULL;
    
    int responseLen;
    char *msg;
    
    long int curLen = 0;
    
    if( accessRes == -1 ) {
        int err = errno;
        if( err == ENOENT ) { // The file does not exist, create it
            fh = fopen( path, "w" );
            fwrite( data, dataLen, 1, fh );
            fclose( fh );
            res = "created";
            goto done;
        }
        else if( err == ENAMETOOLONG ) { // Specified filename is too long
            res = "name too long";
            goto error;
        }
        else {
            char myres[100];
            sprintf( myres, "unknown error: %i", err );
            res = myres;
            //res = "unknown error";
            goto error;
        }
    }
    
    // If diff option is specified; output to a temporary location, and use diff tool to save diff
    // TODO
    
    // File exists, save its contents, then overwrite it
    fh = fopen( path, "r+" );
    if( !fh ) {
        printf("Could not open %s for r+\n", path );
        exit(1);
    }
    
    // Find length of file
    fseek( fh, 0, SEEK_END );
    curLen = ftell( fh ); 
    fseek( fh, 0, SEEK_SET ); 
    
    // Read file into memory
    oldData = malloc( curLen );
    fread( oldData, curLen, 1, fh );
    
    // go back to start; write new contents
    fseek( fh, 0, SEEK_SET ); 
    fwrite( data, dataLen, 1, fh );
    
    // truncate if needed if it used to be longer
    if( dataLen < curLen ) {
        if( ftruncate( fileno( fh ), dataLen ) != 0 ) {
            res = "content written; but could not truncate";
            fclose( fh );
            goto error;
        }
    }
    fclose( fh );
    goto done;
    
error:
    responseLen = 20 + 20 + 30 + 22 // Size of template ( see below )
                  + 20 // some extra to handle integer expansion
                  ;
    responseLen += strlen( res );
    msg = malloc( responseLen );
    snprintf( msg, responseLen, 
        "<result itemId='%i'>" // 20
        "<localvars>" // 20
        "<error><![CDATA[%s]]></error>" // ~30
        "</localvars></result>", // ~22
        itemId,
        res );
        
done:
    responseLen = 20 + 20 + 30 + 22 // Size of template ( see below )
                  + 20 // some extra to handle integer expansion
                  ;
    responseLen += strlen( res );
    
    if( oldData ) {
        responseLen += curLen;
        msg = malloc( responseLen );
        snprintf( msg, responseLen, 
            "<result itemId='%i'>" // 20
            "<localvars res='%s'>" // 20
            "<oldData><![CDATA[%.*s]]></oldData>" // ~30
            "</localvars></result>", // ~22
            itemId,
            res,
            (int) curLen,
            oldData );
        free( oldData );
    }
    else {
        responseLen -= 30;
        msg = malloc( responseLen );
        snprintf( msg, responseLen, 
            "<result itemId='%i'>" // 20
            "<localvars res='%s'>" // 20
            "</localvars></result>", // ~22
            itemId,
            res,
            curLen,
            oldData );
    }
        
    return msg;
}