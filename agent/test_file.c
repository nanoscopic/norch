#include "item_file.h"
#include<xjr-machine.h>
#include<string.h>
#include<stdio.h>

int main( int argc, char *argv[] ) {
    xjr_node__disable_mempool();
    char *testItemSrc = "<file path='testfile' data='blah'/>";
    xjr_node *root = parse( 0, testItemSrc, strlen( testItemSrc ) );
    xjr_node__dump( root, 20 );
    char *id = "10";
    char *idz = strdup( id );
    char *res = item_file( xjr_node__get( root, "file", 4 ), idz );
    printf(res);
}