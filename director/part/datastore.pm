package part::datastore;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;

#my $socket_address = "inproc://datastore";
my $socket_address = "tcp://127.0.0.1:8300";
my $curItemId = 1;
my $datastoreContext = { name => 'datastore' };

sub dofork {
    my $pid = fork();
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item ) = @_;
    my $request = "<new_item><item><![CDATA[$item]]></item></new_item>";
    return raw_request( $context, $request );
}

sub get_agent_addr {
    my ( $context, $agentName ) = @_;
    my $request = "<get_agent_addr agentName='$agentName'/>";
    return raw_request( $context, $request );
}

sub raw_request {
    my ( $context, $request ) = @_;
    my $socket = get_socket( $context );
    
    print "Sending '$request' to datastore\n";
    my $sentBytes = nn_send( $socket, $request, 0 );
    if( $sentBytes == -1 ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "failed to send: $err";
    }
    print "Sent $sentBytes bytes\n";
    my $bytes = nn_recv( $socket, my $reply, 5000, 0 );
    if( $bytes == -1 ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "failed to recv: $err";
    }
    print "Raw request to datastore got back $bytes bytes\n";
    return $reply;
}

sub get_socket {
    my $context = shift;
    my $socket = $context->{'datastore_socket'};
    if( $socket ) { return $socket; }
    return doconnect( $context );
}

sub doconnect {
    my $context = shift;
    
    my $cName = $context->{'name'};
    print "Connecting to datastore in context $cName\n";
    my $socket = nn_socket(AF_SP, NN_REQ);
    #print "Listening for datastore requests on $socket_address\n";
    my $bindok = nn_connect($socket, $socket_address);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = part::misc::decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket, NN_SOL_SOCKET, NN_SNDTIMEO, 2000 ); # timeout send in 2000ms
    $context->{'datastore_socket'} = $socket;
    return $socket;
}

my %agents;
my @items;
my %itemHash;

sub handle_datastore_item {
    my ( $buffer, $size ) = @_;
    print "Datastore Received '$buffer'\n\n";
    my $root = Parse::XJR->new( text => $buffer );
    my $req = $root->firstChild();
    my $type = $req->name();
    
    print "  Type=$type\n";
    if( $type eq 'new_item' ) {
        my $raw_item = $req->{item}->value();
        print "Raw item: '$raw_item'\n";
        my $root2 = Parse::XJR->new( text => $raw_item );
        my $item = $root2->firstChild();
        $item->dump( 20 );
        my $itemId = $curItemId++;
        $item->{id} = $itemId;
        $itemHash{ $itemId } = $item;
        #$item->dump( 20 );
        push( @items, [ $raw_item, $item ] );
        print "  Returning $itemId\n";
        return $itemId;
    }
    elsif( $type eq 'get_agent_addr' ) {
        my $agentName = $req->{agentName}->value();
        my $agent = $agents{ $agentName };
        return $agent->{address};
    }
    elsif( $type eq 'add_agent' ) {
        my $agentName = $req->{name}->value();
        my $address = $req->{address}->value();
        $agents{$agentName} = {
            address => $address
        };
        return "<result>agent $agentName added</result>";
    }
    elsif( $type eq 'list_agents' ) {
        my $result = '';
        for my $agentName ( keys %agents ) {
            my $agent = $agents{ $agentName };
            my $addr = $agent->{address};
            $result .= "$agentName\n  address=$addr\n";
        }
        return "<result>$result</result>";
    }
    return 'none';
}

sub dolisten {
    my $socket_in = nn_socket(AF_SP, NN_REP);
    print "Listening for datastore requests on $socket_address\n";
    my $bindok = nn_bind($socket_in, $socket_address);
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
                print 'DS ';
                next;
            }
            $err = part::misc::decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        print "DS received $bytes bytes\n";
        my $startTime = [ gettimeofday() ];
        my $response = handle_datastore_item( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        print "DS responding with $response\n";
        nn_send( $socket_in, $response, 0 );
        my $sent_bytes = length( $response );
        
        #logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

1;