#include<nanomsg/nn.h>
#include<nanomsg/pipeline.h>
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>

char *decode_err( int err );

int main( const int argc, const char **argv ) {
    char *socket_address_in = (char *) "ipc:///var/www/html/wcm3/socket/client.ipc";
    char *socket_address_out = (char *) "ipc:///var/www/html/wcm3/socket/melon.ipc";
    char *socket_address_out2 = (char *) "ipc:///var/www/html/wcm3/socket/matt.ipc";
    char *socket_address_out3 = (char *) "ipc:///var/www/html/wcm3/socket/carbon.ipc";
    char *socket_address_out4 = (char *) "ipc:///var/www/html/wcm3/socket/wcm4.ipc";
    
    umask(0);
    int socket_in = nn_socket(AF_SP, NN_PULL);
    int bind_res = nn_bind(socket_in, socket_address_in);
    if( bind_res < 0 ) {
        printf("Bind in error\n");
        exit(1);
    }
    
    int send_timeout = 200;
    
    int socket_out = nn_socket(AF_SP, NN_PUSH);
    bind_res = nn_bind(socket_out, socket_address_out);
    nn_setsockopt( socket_out, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof(send_timeout) );
    if( bind_res < 0 ) {
        printf("Bind out error\n");
        exit(1);
    }
    
    int socket_out2 = nn_socket(AF_SP, NN_PUSH);
    bind_res = nn_bind(socket_out2, socket_address_out2);
    nn_setsockopt( socket_out2, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof(send_timeout) );
    if( bind_res < 0 ) {
        printf("Bind out error\n");
        exit(1);
    }
    
    int socket_out3 = nn_socket(AF_SP, NN_PUSH);
    bind_res = nn_bind(socket_out3, socket_address_out3);
    nn_setsockopt( socket_out3, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof(send_timeout) );
    if( bind_res < 0 ) {
        printf("Bind out error\n");
        exit(1);
    }
    
    int socket_out4 = nn_socket(AF_SP, NN_PUSH);
    bind_res = nn_bind(socket_out4, socket_address_out4);
    nn_setsockopt( socket_out4, NN_SOL_SOCKET, NN_SNDTIMEO, &send_timeout, sizeof(send_timeout) );
    if( bind_res < 0 ) {
        printf("Bind out error\n");
        exit(1);
    }
    
    while(1) {
        void *buf = NULL;
        int bytes = nn_recv(socket_in, &buf, NN_MSG, 0);
        if( bytes < 0 ) {
           int err = errno;
           char *errStr = decode_err( err );
           printf( "failed to receive: %s\n",errStr);
           delete errStr;
           continue;
        }
        
        printf("Incoming Bytes: %i, Buffer:%s\n\n", bytes, (char *) buf );
        
        int socket_dest = socket_out;
        char *pathpos = strstr( (char *) buf, "path='" );
        if( pathpos ) {
            pathpos += 6;
            if( !strncmp( pathpos, "/wcm2", 5 ) ) {
                socket_dest = socket_out2;
            }
            if( !strncmp( pathpos, "/wcm4", 5 ) ) {
                socket_dest = socket_out4;
            }
            if( !strncmp( pathpos, "/x", 2 ) ) {
                socket_dest = socket_out3;
            }
        }
        
        int sent_bytes = nn_send(socket_dest, buf, bytes, 0 );
        if( !sent_bytes ) {
            int err = errno;
            char *errStr = decode_err( err );
            printf("failed to resend: %s\n",errStr);
            delete errStr;
            nn_freemsg( buf );
            continue;
        }
    }
}

char *decode_err( int err ) {
    char *buf;
    if( err == EBADF ) { return strdup("EBADF"); }
    if( err == ENOTSUP ) { return strdup("ENOTSUP"); }
    if( err == EFSM ) { return strdup("EFSM"); }
    if( err == EAGAIN ) { return strdup("EAGAIN"); }
    if( err == EINTR ) { return strdup("EINTR"); }
    if( err == ETIMEDOUT ) { return strdup("ETIMEDOUT"); }
    buf = new char[100];
    sprintf(buf,"Err number: %i", err );
    return buf;
}
