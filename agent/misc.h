#ifndef __MISC_H
#define __MISC_H
char *decode_err( int err );
enum bindtype{bind, connect}; 
int make_nn_socket( char *address, enum bindtype btype, int type, int send_timeout, int recv_timeout );

typedef struct split_line_s split_line;

struct split_line_s {
    char *line;
    int len;
    split_line *next;
};

split_line *split_lines( char *data, long int dataLen, int *lcountP );

#endif