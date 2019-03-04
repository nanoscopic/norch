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
        part::datastore::raw_request( $resultsContext, $buffer );
        
        # Inform the scheduler that the item is finished
        #   so that following tasks can be triggered
        part::scheduler::item_finished( $itemId );
        
        #my $localvars = $result->{localvars};
        #my $localVarXJR = $localvars->outerxjr();
        #my $dsreq = "<item_results itemId=$itemId>$localVarXJR</item_results>";
        #part::datastore::raw_request( $resultsContext, $dsreq );
        
        # Store local variables that were output
        #my $curVarNode = $localvars->firstChild();
        #while( $curVarNode ) {
        #    my $curVar = $curVarNode->name();
        #    
        #    $curVarNode = $curVarNode->next();
        #}
        
        # Store global variables that were output
        #my $globalvars = $result->{globalvars};
        #$curVarNode = $globalvars->firstChild();
        #while( $curVarNode ) {
        #    my $curVar = $curVarNode->name();
        #    
        #    $curVarNode = $curVarNode->next();
        #}
        
        # Store the results here ( note we don't store in the 'datastore' )
        # Notify the datastore though that the item is 'done'
        # Notify the scheduler as well, so that follow up items can be done
        
        # It may be necessary to process the results to store the results into variables for
        #   use by subsequent items. The result variables though should be stored into the
        #   datastore, so perhaps the scheduler should be notified by the datastore once
        #   it has actually stored the results. It would be bad if the scheduler was notified
        #   and scheduled a following task before the variables needed by the following
        #   task are stored in the datastore.
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