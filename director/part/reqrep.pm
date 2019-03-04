package part::reqrep;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;
use part::scheduler;

my $cmdId = 1;

my $reqrepContext = { name => 'reqrep' };

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub dolisten {
    my $socket_address_in = "tcp://127.0.0.1:8275";
    umask(0);
    my $socket_in = nn_socket(AF_SP, NN_REP);
    print "Listening for requests on $socket_address_in\n";
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
                print 'RR ';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $response = handle_incoming_xjr( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        nn_send( $socket_in, $response, 0 );
        my $sent_bytes = length( $response );
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

sub add_cmd {
    my $node = shift;
    my $id = $cmdId++;
    my $agent = {
        id => $id,
        node => $node
    };
    return $id;
}

sub handle_incoming_xjr {
    my ( $rawXjr, $bytes ) = @_;
    print "\nReqrep Received '$rawXjr'\n\n";
    
    my $root = Parse::XJR->new( text => $rawXjr );
    my $curChild = $root->firstChild();
    
    my $res = '';
    while( $curChild ) {
        $res .= handle_item( $curChild );
        $curChild = $curChild->next();
    }
    return $res;
}

sub handle_item {
    my $item = shift;
    my $type = $item->name();
    my $rawItem = "<$type>" . $item->xjr() . "</$type>";
        
    if( $type eq 'cmd' ) {
        print "Reqrep - Cmd incoming\n";
        
        # Forward the command to the datastore to track results
        my $item_id = part::datastore::new_item( $reqrepContext, $rawItem );
        
        print "New new id: $item_id\n";
        # Forward the command to the scheduler
        part::scheduler::new_item( $reqrepContext, $rawItem, $item_id );
    }
    elsif( $type eq 'add_agent' ) {
        print "Reqrep - Adding agent\n";
        return part::datastore::raw_request( $reqrepContext, $rawItem );
    }
    elsif( $type eq 'list_agents' ) {
        print "Reqrep - Listing agents\n";
        return part::datastore::raw_request( $reqrepContext, $rawItem );
    }
    else {
        print "Unknown item: $type\n";
    }

    return "<result>test</result>";
}

sub send_cmd {
    my ( $node, $dest ) = @_;
    # connect to the node and send it the request
}

1;