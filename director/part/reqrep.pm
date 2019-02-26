package part::reqrep;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;

my $cmdId = 1;

my $data = {};

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
                print '.';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $res = handle_data_req( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        my $response = $res->{'response'};
        nn_send( $socket_in, $response, 0 );
        my $sent_bytes = length( $response );
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

my %agents;

sub add_cmd {
    my $node = shift;
    my $id = $cmdId++;
    my $agent = {
        id => $id,
        node => $node
    };
    return $id;
}

sub handle_data_req {
    my ( $buffer, $bytes ) = @_;
    print "\nReceived '$buffer'\n";
    print "\n";
    if( $buffer ) {
        my $root = Parse::XJR->new( text => $buffer );
        my $req = $root->{'req'};
        my $type = $req->{'type'}->value();
        
        if( $type eq 'cmd' ) {
            # Forward the command to the scheduler
            part::scheduler::new_item( $data, $buffer );
            
            # Forward the command to the datastore to track results
            part::datastore::new_item( $data, $buffer );
            
            #my $node = $req->{node}->value();
            #print "Queuing Command request\n  node=$node\n";
            #my $cmd = $req->{'cmd'}->value();
            #print "  cmd = $cmd\n";
            #my $id = add_cmd( $cmd );
            #print "  id = $id\n";
            #$req->{id} = $id;
            #send_cmd( $req, $node );
        }
        elsif( $type eq 'addagent' ) {
            print "Adding node\n";
            my $name = $req->{name}->value();
            my $address = $req->{address}->value();
            $agents{$name} = {
                address => $address
            };
            return { response => "<result>agent $name added</result>" };
        }
        elsif( $type eq 'listnodes' ) {
            my $result = '';
            for my $agentName ( keys %agents ) {
                my $agent = $agents{ $agentName };
                my $addr = $agent->{address};
                $result .= "$agentName\n  address=$addr\n";
            }
            return { response => "<result>$result</result>" };
        }
        else {
            print "Unknown req: $type\n";
        }
    }
    return { response  => "<result>test</result>" };
}

sub send_cmd {
    my ( $node, $dest ) = @_;
    # connect to the node and send it the request
}

1;