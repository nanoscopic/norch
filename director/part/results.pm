package part::results;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;

sub dolisten {
    my $socket_address_in = "tcp://127.0.0.1:8274";
    umask(0);
    my $socket_in = nn_socket(AF_SP, NN_PULL);
    print "Listening for results on $socket_address_in\n";
    my $bindok = nn_bind($socket_in, $socket_address_in);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket_in, NN_SOL_SOCKET, NN_RCVTIMEO, 2000 ); # timeout receive in 200ms
    
    local $| = 1; # autoflush so we can see the dots
    while( 1 ) {
        my $bytes = nn_recv( $socket_in, my $buf, 5000, 0 );
        if( !$bytes ) {
            my $err = nn_errno();
            if( $err == ETIMEDOUT ) {
                print '.';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $res = handle_data( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        my $sent_bytes = $res->{'sent_bytes'};
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

sub handle_data {
    my ( $buffer, $bytes ) = @_;
    return { sent_bytes => 0 };
}

1;