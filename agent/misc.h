#ifndef __MISC_H
#define __MISC_H
char *decode_err( int err );
enum bindtype{bind, connect}; 
int make_nn_socket( char *address, enum bindtype btype, int type, int send_timeout, int recv_timeout );
#endif