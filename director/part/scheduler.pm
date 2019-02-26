package part::scheduler;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;

my $socket_address = "inproc://scheduler";
my @items;

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item ) = @_;
    my $request = "<req type='new_item'><item><![CDATA[$item]]></item></req>";
    request( $context, $request );
}

sub request {
    my ( $context, $request ) = @_;
    my $socket = $context->{'scheduler_socket'};
    nn_send( $socket, $in, 0 );
    my $bytes = nn_recv( $socket, my $buf, 5000, 0 );
    return $reply;
}

sub doconnect {
    my $context = shift;
    my $socket_address_in = "inproc://scheduler";
    
    my $socket_out = nn_socket(AF_SP, NN_REQ);
    #print "Listening for datastore requests on $socket_address\n";
    my $bindok = nn_connect($socket_in, $socket_address);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket_in, NN_SOL_SOCKET, NN_SENDTIMEO, 2000 ); # timeout send in 2000ms
    $context->{'scheduler_socket'} = $socket_out;
}

sub handle_scheduler_req {
    my ( $buffer, $size ) = @_;
    my $req = Parse::XJR->new( text => $buffer );
    $req = $req->{req};
    my $type = $req->{type}->value();
    if( $type eq 'new_item' ) {
        my $raw_item = $req->{item}->value();
        my $item = Parse::XJR->new( text => $raw_item );
        push( @items, [ $raw_item, $item ] );
    }
    return { response => 'none' };
}

sub send_items {
    while( @items ) {
        my $itemParts = shift @items;
        my $raw_item = $itemParts->[0];
        my $item = $itemParts->[1];
        
        # determine the socket to send to the item
        # looking first in a cache of open destinations
        # if we don't have the destination socket open/cached, use cache of destination addr
        # if we don't have the destination address, get it from the datastore
        
    }
}

sub dolisten {
    my $socket_address = "inproc://scheduler";
    my $socket_in = nn_socket(AF_SP, NN_REP);
    print "Listening for datastore requests on $socket_address\n";
    my $bindok = nn_bind($socket_in, $socket_address);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket_in, NN_SOL_SOCKET, NN_RCVTIMEO, 1000 ); # timeout receive in 1000ms
    
    local $| = 1; # autoflush so we can see the dots
    while( 1 ) {
        if( @items ) {
            send_items();
        }
        
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
        my $res = handle_scheduler_req( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        my $response = $res->{'response'};
        nn_send( $socket_in, $response, 0 );
        my $sent_bytes = length( $response );
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

1;