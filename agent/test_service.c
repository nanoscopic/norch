#include "item_service.h"
#include<xjr-machine.h>
#include<string.h>
#include<stdio.h>

int main( int argc, char *argv[] ) {
    xjr_node__disable_mempool();
    char *testItemSrc = "<service unit='test' action='start'/>";
    xjr_node *root = parse( 0, testItemSrc, strlen( testItemSrc ) );
    xjr_node__dump( root, 20 );
    char *id = "10";
    char *idz = strdup( id );
    char *res = item_service( xjr_node__get( root, "service", 7 ), idz );
    printf(res);
}