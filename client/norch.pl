#!/usr/bin/perl -w
use strict;
use NanoMsg::Raw;

my $socket_out = connect_director();
my $out = $ARGV[0];
print "Sending $out\n";
my $response = request( $socket_out, $out );
print "Received $response\n";

sub connect_director {
    my $socket_address_out = "tcp://127.0.0.1:8275";
    umask(0);
    my $socket_out = nn_socket(AF_SP, NN_REQ);
    print "Listening on $socket_address_out\n";
    my $bindok = nn_connect($socket_out, $socket_address_out);
    if( !$bindok ) {
        my $err = nn_errno();
        #if( $err == EADDRINUSE ) {
        #    my $connectok = nn_connect($socket_in, $socket_address_in);
        #    if( !$connectok ) {
        #        die "could not connect after fail on bind";
        #    }
        #}
        #else {
            $err = decode_err( $err );
            die "fail to connect: $err";
        #}
    }
    nn_setsockopt( $socket_out, NN_SOL_SOCKET, NN_RCVTIMEO, 2000 ); # timeout receive in 2000ms
    return $socket_out;
}

sub request {
    my ( $socket, $req ) = @_;
    
    nn_send( $socket, $req, 0 );

    my $buffer;
    
    nn_recv( $socket, $buffer, 100, 0 );
    
    return $buffer;
}