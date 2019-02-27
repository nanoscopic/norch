#include "item_cmd.h"
#include<xjr-machine.h>

int main( int argc, char *argv[] ) {
    xjr_node__disable_mempool();
    char *testItemSrc = "<cmd arg=['-l','norcfh.c'] cmd='/bin/ls'/>";
    xjr_node *root = parse( 0, testItemSrc, strlen( testItemSrc ) );
    xjr_node__dump( root, 20 );
    char *id = "10";
    char *idz = strdup( id );
    item_cmd( xjr_node__get( root, "cmd", 3 ), idz );
}