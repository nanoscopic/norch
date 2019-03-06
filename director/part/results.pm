package part::results;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;

my $resultsContext = { name => 'results' };

sub handle_results_item {
    my ( $buffer, $size ) = @_;
    print "Received result: '$buffer'\n";
    my $root = Parse::XJR->new( text => $buffer );
    my $req = $root->firstChild();
    my $type = $req->name();
    my $itemId = $req->{itemId}->value();
    if( $type eq 'result' ) {
        # Example result message:
        # <result itemId='123'>
        #   <localvars errorCode='0'>
        #     <stdout>...</stdout>
        #     <stderr>...</stderr>
        #   </localvars>
        #   <globalvars>
        #     <myglobal>1</myglobal>
        #   </globalvars>
        # </result>
        
        my $result = $req; # to make it less confusing
        
        # Forward on to the datastore; to store the results of the item
        # This just stores it internally to be processed by the datastore thread then returns immediately
        part::datastore::raw_request( $resultsContext, $buffer );
        
        # The datastore will do the following itself once it has finished processing the results.
        # part::scheduler::item_finished( $resultsContext, $itemId );
    }
}

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
    nn_setsockopt( $socket_in, NN_SOL_SOCKET, NN_RCVTIMEO, 500 ); # timeout receive in 200ms
    
    local $| = 1; # autoflush so we can see the dots
    while( 1 ) {
        my $bytes = nn_recv( $socket_in, my $buf, 20000, 0 );
        if( !$bytes ) {
            my $err = nn_errno();
            if( $err == ETIMEDOUT ) {
                print 'RS ';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $response = handle_results_item( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        #my $sent_bytes = $res->{'sent_bytes'};
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

1;