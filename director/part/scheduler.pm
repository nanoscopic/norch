package part::scheduler;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;

my $socket_address = "inproc://scheduler";
my @items;
my $schedulerContext = {};

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item, $itemId ) = @_;
    my $request = "<req type='new_item' itemId='$itemId'><item><![CDATA[$item]]></item></req>";
    raw_request( $context, $request );
}

sub raw_request {
    my ( $context, $request ) = @_;
    my $socket = get_socket( $context );
    nn_send( $socket, $request, 0 );
    my $bytes = nn_recv( $socket, my $buf, 5000, 0 );
    return $reply;
}

sub get_socket {
    my $context = shift;
    my $socket = $context->{'scheduler_socket'};
    if( $socket ) { return $socket; }
    return doconnect( $context );
}

sub doconnect {
    my $context = shift;
    
    my $socket = nn_socket(AF_SP, NN_REQ);
    #print "Listening for datastore requests on $socket_address\n";
    my $bindok = nn_connect($socket, $socket_address);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket, NN_SOL_SOCKET, NN_SENDTIMEO, 2000 ); # timeout send in 2000ms
    $context->{'scheduler_socket'} = $socket;
    return $socket;
}

sub handle_scheduler_req {
    my ( $buffer, $size ) = @_;
    my $req = Parse::XJR->new( text => $buffer );
    $req = $req->{req};
    my $type = $req->{type}->value();
    if( $type eq 'new_item' ) {
        my $raw_item = $req->{item}->value();
        my $itemId = $req->{itemId}->value();
        my $item = Parse::XJR->new( text => $raw_item );
        push( @items, [ $raw_item, $item, $itemId ] );
    }
    return { response => 'none' };
}

# Cache of destination sockets and addresses
my %dest_socket;
my %dest_addr;

sub agent_name_to_socket {
    my $agent = shift;
    
    # looking first in a cache of open destinations
    if( defined $dest_socket{ $agent } ) {
        return $dest_socket{ $agent };
    }
    
    # if we don't have the destination socket open/cached, use cache of destination addr
    elsif( defined $dest_addr{ $agent } ) {
        my $dest_socket = $dest_socket{ $agent } = open_node_socket( $dest_addr{ $agent } );
        return $dest_socket;
    }
        
    # if we don't have the destination address, get it from the datastore
    my $dest_addr = $dest_addr{ $agent } = part::datastore::get_agent_addr( $schedulerContext, $agent );
    my $dest_socket = $dest_socket{ $agent } = open_node_socket( $dest_addr );
    return $dest_socket;
}

sub send_items {
    while( @items ) {
        my $itemParts = shift @items;
        my $raw_item = $itemParts->[0];
        my $item = $itemParts->[1];
        my $itemId = $itemParts->[2];
        my $agent = $item->{agent}->value();
        
        # determine the socket to send to the item
        my $socket = agent_name_to_socket( $agent );
        
        nn_send( $socket, "$raw_item<extra itemId='$itemId'/>" );
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