package part::scheduler;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;

#my $socket_address = "inproc://scheduler";
my $socket_address = "tcp://127.0.0.1:8301";
my @items;
my $schedulerContext = { name => 'scheduler' };

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item, $itemId ) = @_;
    my $request = "<new_item itemId='$itemId'><item><![CDATA[$item]]></item></new_item>";
    raw_request( $context, $request );
}

sub raw_request {
    my ( $context, $request ) = @_;
    
    print "Sending '$request' to scheduler\n";
    
    my $socket = get_socket( $context );
    my $sentBytes = nn_send( $socket, $request, 0 );
    if( $sentBytes == -1 ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "failed to send: $err";
    }
    my $bytes = nn_recv( $socket, my $reply, 5000, 0 );
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
    nn_setsockopt( $socket, NN_SOL_SOCKET, NN_SNDTIMEO, 2000 ); # timeout send in 2000ms
    $context->{'scheduler_socket'} = $socket;
    return $socket;
}

sub handle_scheduler_item {
    my ( $buffer, $size ) = @_;
    print "Scheduler received '$buffer'\n";
    my $root = Parse::XJR->new( text => $buffer );
    my $req = $root->firstChild();
    my $type = $req->name();
    
    print "  type=$type\n";
    if( $type eq 'new_item' ) {
        my $raw_item = $req->{item}->value();
        my $itemId = $req->{itemId}->value();
        my $root2 = Parse::XJR->new( text => $raw_item );
        my $item = $root2->firstChild();
        push( @items, [ $root2, $raw_item, $item, $itemId ] );
    }
    return 'none';
}

# Cache of destination sockets and addresses
my %dest_socket;
my %dest_addr;

sub open_node_socket {
    my ( $address, $agent ) = @_;
    
    my $socket = nn_socket(AF_SP, NN_PUSH);
    print "Attempting to connect to agent $agent on $address\n";
    my $bindok = nn_connect($socket, $address);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket, NN_SOL_SOCKET, NN_SNDTIMEO, 1000 ); # timeout send in 1000ms
    
    return $socket;
}

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
    my $dest_socket = $dest_socket{ $agent } = open_node_socket( $dest_addr, $agent );
    return $dest_socket;
}

sub send_items {
    print "Attempting to send items\n";
    while( my $itemParts = shift @items ) {
        print "ITEM\n";
        my $root = $itemParts->[0];
        my $raw_item = $itemParts->[1];
        my $item = $itemParts->[2];
        my $itemId = $itemParts->[3];
        $item->dump(20);
        my $agent = $item->{agent}->value();
        
        # determine the socket to send to the item
        my $socket = agent_name_to_socket( $agent );
        
        my $toSend = "$raw_item<extra itemId='$itemId'/>";
        print "Sending '$toSend' to agent $agent\n";
        nn_send( $socket, $toSend );
    }
    
    print "Done sending items\n";
}

sub dolisten {
    my $socket_in = nn_socket(AF_SP, NN_REP);
    print "Listening for scheduler requests on $socket_address\n";
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
                print 'SC ';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $response = handle_scheduler_item( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        print "scheduler responding with '$response'\n";
        my $sent_bytes = nn_send( $socket_in, $response, 0 );
        if( $sent_bytes == -1 ) {
            my $err = nn_errno();
            $err = part::misc::decode_err( $err );
            die "failed to send: $err";
        }
        print "Scheduler responded $sent_bytes bytes\n";
        #my $sent_bytes = length( $response );
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

1;