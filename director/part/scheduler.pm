package part::scheduler;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;
use Template::Nepl;

#my $socket_address = "inproc://scheduler";
my $socket_address = "tcp://127.0.0.1:8301";
my @items;
my %itemsById;
my %done_ids; # ids of items that are done 
my %waiting; # items waiting on others to be done; stored by the name they are waiting on
my $schedulerContext = { name => 'scheduler' };
my $nepl = Template::Nepl->new( lang => 'perl', pkg => 'part::template' );

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item, $itemId ) = @_;
    $item =~ s/]]>/\|\|>/g;
    my $request = "<new_item itemId='$itemId'><item><![CDATA[$item]]></item></new_item>";
    raw_request( $context, $request );
}

sub item_finished {
    my ( $context, $itemId ) = @_;
    my $request = "<item_finished itemId='$itemId'/>";
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

sub type_new_item {
    my $req = shift;
    my $raw_item = $req->{item}->value();
    $raw_item =~ s/\|\|>/]]>/g;
    my $itemId = $req->{itemId}->value();
    print "scheduler.type_new_item - queueing itemId $itemId\n";
    my $root2 = Parse::XJR->new( text => $raw_item );
    my $item1 = $root2->firstChild();
    my $itemName = '';
    if( $item1->{name} ) {
        $itemName = $item1->{name}->value();
    }
    my $dep = $item1->{dep} ? $item1->{dep}->value() : '';
    my $ready = 0;
    
    my $item = {
        root => $root2,
        raw => $raw_item,
        item => $item1,
        id => $itemId,
        ready => $ready,
        name => $itemName
    };
    
    if( $dep ) {
        print "Setting item to wait on dep $dep\n";
        $waiting{ $dep } = $item;
    }
    else {
        $item->{ready} = 1;
    }
    
    $itemsById{ $itemId } = $item;
    push( @items, $item );
}

sub type_item_finished {
    my $req = shift;
    my $itemId = $req->{itemId}->value();
    my $ob = $itemsById{ $itemId };
    $done_ids{ $itemId } = 1;
    my $itemName = $ob->{name};
    print "Name of item that finished: $itemName\n";
    #use Data::Dumper;
    #print Dumper( \%waiting );
    my $waitingOb = $waiting{ $itemName };
    if( $waitingOb ) {
        print "Found a waiting item; marking it ready\n";
        my $item = $waitingOb->{item};
        print $item->outerxjr();
        #$item->dump(20);
        $waitingOb->{ready} = 1;
    }
    # Find items that depend on this one and start them
}

sub handle_scheduler_item {
    my ( $buffer, $size ) = @_;
    print "Scheduler received '$buffer'\n";
    my $root = Parse::XJR->new( text => $buffer );
    my $req = $root->firstChild();
    my $type = $req->name();
    
    print "  type=$type\n";
    if   ( $type eq 'new_item'      ) { type_new_item( $req ); }
    elsif( $type eq 'item_finished' ) { type_item_finished( $req ); }
    elsif( $type eq 'teval_done'    ) { type_teval_done( $req ); }
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

# Template eval
sub teval {
    my ( $source, $itemId ) = @_;
    
    # Rather than porting variables and results between the scheduler and the datastore,
    #   we just send the template over to the datastore and let it be executed there.
    
    # TODO: Instead of getting the evaluated template back from the datastore and sending
    #   it to the agents then; just send it directly to the agents from the datastore;
    #   that way it can all be done asynchronously.
    return part::datastore::teval_with_vars( $schedulerContext, $source, $itemId );
    
    #my $tpl = $nepl->fetch_template( source => $tpl );
    #return eval( $tpl->{'code'} );
}

sub type_teval_done {
    my $item = shift;
    my $rawItem = $item->{rawItem}->value();
    $rawItem =~ s/\|\|>/]]>/g;
    my $itemId = $item->{itemId}->value();
    print "scheduler.type_teval_done - start itemId:$itemId\n";
    my $root = Parse::XJR->new( text => $rawItem );
    my $ob = {
        root => $root,
        raw => $rawItem,
        item => $root->firstChild(),
        id => $itemId
    };
    send_item( $ob );
}

sub send_item {
    my $ob = shift;
    my $root = $ob->{root};
    my $raw_item = $ob->{raw};
    my $item = $ob->{item};
    my $itemId = $ob->{id};
    print "scheduler.send_item - start itemId $itemId\n";
    
    # TODO: When the item is sent, look through its XJR representation and determine if it contains Template::Nepl
    # expressions. If it does, evaluate those before sending so that it can utilize the results of previous items.
    # This cannot be done for a 'foreach' loop, as it is not desired to run the expressions on each item until
    # going through the loop.
    if( $raw_item =~ m/\*\{/ ) {
        teval( $raw_item, $itemId );
        return;
        #$raw_item = $ob->{raw} = teval( $raw_item );
        #$item = $ob->{item} = Parse::XJR->new( text => $raw_item );
    }
    
    my $itemType = $item->name();
    print "scheduler.send_item - itemType $itemType\n";
    if( $itemType eq 'output' ) {
        my $tpl = $item->{tpl}->value();
        print "Output: $tpl\n";
        return;
    }
    
    #$item->dump(20);
    my $agent = $item->{agent}->value();
    
    # determine the socket to send to the item
    my $socket = agent_name_to_socket( $agent );
    
    my $toSend = "$raw_item<extra itemId='$itemId'/>";
    print "scheduler.send_type - Sending '$toSend' to agent $agent\n";
    nn_send( $socket, $toSend );
}

sub send_items {
    #print "scheduler.send_items - start\n";
    
    # Find the index of the first "ready item"
    while( 1 ) {
        my $acted = 0;
        for( my $i=0;$i<scalar(@items);$i++ ) {
            my $ob = $items[ $i ];
            if( $ob->{'ready'} ) {
                send_item( $ob );
                $acted = 1;
                splice @items, $i, 1;
                last;
            }
        }
        last if( !$acted );
    }
    
    #print "scheduler.send_items - end\n";
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