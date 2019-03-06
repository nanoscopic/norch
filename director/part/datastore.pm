package part::datastore;
use strict;
use warnings;
use NanoMsg::Raw;
use lib '..';
use part::misc;
use Time::HiRes qw/gettimeofday tv_interval/;
use Template::Nepl;

#my $socket_address = "inproc://datastore";
my $socket_address = "tcp://127.0.0.1:8300";
my $curItemId = 1;
my $datastoreContext = { name => 'datastore' };
my $nepl; # only assigned after we fork

sub dofork {
    my $pid = fork();
    $nepl = Template::Nepl->new( lang => 'perl', pkg => 'part::template' );
    if( $pid == 0 ) { dolisten(); exit(0); }
}

sub new_item {
    my ( $context, $item ) = @_;
    $item =~ s/]]>/\|\|>/g;
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
my %itemHash; # stored by id
my %itemVars; # stored by name
my %globalVars;
my %results;

sub type_new_item {
    my $req = shift;
    my $raw_item = $req->{item}->value();
    $raw_item =~ s/\|\|>/]]>/g;
    print "datastore.type_new_item - Raw item: '$raw_item'\n";
    my $root2 = Parse::XJR->new( text => $raw_item );
    my $item = $root2->firstChild();
    $item->dump( 20 );
    my $itemId = $curItemId++;
    $item->{id} = $itemId;
    $itemHash{ $itemId } = { raw => $raw_item, item => $item, root => $root2 };
    #$item->dump( 20 );
    push( @items, [ $raw_item, $item ] );
    print "datastore.type_new_item - Returning $itemId\n";
    return $itemId;
}

sub type_get_agent_addr {
    my $req = shift;
    my $agentName = $req->{agentName}->value();
    my $agent = $agents{ $agentName };
    return $agent->{address};
}

sub type_add_agent {
    my $req = shift;
    my $agentName = $req->{name}->value();
    my $address = $req->{address}->value();
    $agents{$agentName} = {
        address => $address
    };
    return "<result>agent $agentName added</result>";
}

# Evaluate an templatized item 
sub teval_with_vars {
    my ( $context, $rawItem, $itemId ) = @_;
    $rawItem =~ s/]]>/\|\|>/g;
    my $request = "<teval itemId='$itemId'><rawItem><![CDATA[$rawItem]]></rawItem></teval>";
    print "datastore.teval_with_vars created item:\n" . $request . "\n";
    return raw_request( $context, $request );
}

sub type_list_agents {
    my $req = shift;
    my $result = '';
    for my $agentName ( keys %agents ) {
        my $agent = $agents{ $agentName };
        my $addr = $agent->{address};
        $result .= "$agentName\n  address=$addr\n";
    }
    return "<result>$result</result>";
}

sub type_result {
    my $result = shift;
    
    my $itemId = $result->{itemId}->value();
    print "datastore.type_result - start itemId $itemId\n";
    
    # Why not just store the entire node for later :)
    #$results{ "$itemId" } = $result;
    
    my $item = $itemHash{ $itemId }->{item};
    
    if( !$item ) {
        print "datastore has no item with itemId $itemId\n";
        die;
    }
    my $itemName = $item->{name} ? $item->{name}->value() : '';
    
    print "datastore.type_result - itemName $itemName\n";
    # Store local variables that were output
    if( $itemName ) {
        my $itemVars = {};
        
        my $localvars = $result->{localvars};
        if( $localvars ) {
            my $curVarNode = $localvars->firstChild();
            while( $curVarNode ) {
                my $curVar = $curVarNode->name();
                $itemVars->{ $curVar } = $curVarNode->value();
                $curVarNode = $curVarNode->next();
            }
            $itemVars{ $itemName } = $itemVars;
        }
    }   
    print "datastore.type_result - y\n";
    # Store global variables that were output
    my $globalvars = $result->{globalvars};
    if( $globalvars ) {
        my $curVarNode = $globalvars->firstChild();
        while( $curVarNode ) {
            my $curVar = $curVarNode->name();
            $globalVars{ $curVar } = $curVarNode->value();
            $curVarNode = $curVarNode->next();
        }
    }
    
    print "datastore.type_result - x\n";
    
    # TODO; allow the result to contain an 'expr' that is Template::Nepl
    # evaluated so that various other things within the system can be triggered
    # when the result is processed.
    
    part::scheduler::item_finished( $datastoreContext, $itemId );
    print "datastore.type_result - end itemId $itemId\n";
}

sub type_teval {
    my $teval = shift;
    my $source = $teval->{rawItem}->value();
    my $itemId = $teval->{itemId}->value();
    $source =~ s/\|\|>/]]>/g;
    my $tpl = $nepl->fetch_template( source => $source );
    my $tevalRes = '';
    {
        $tevalRes = eval( $tpl->{'code'} );
    }
    $tevalRes =~ s/]]>/\|\|>/g;
    my $req = "<teval_done itemId='$itemId'><rawItem><![CDATA[$tevalRes]]></rawItem></teval_done>";
    part::scheduler::raw_request( $datastoreContext, $req );
}

my @threadItems;
sub enqueue_thread_item {
    my $rawReq = shift;
    push( @threadItems, $rawReq );
    return 'queued';
}

sub process_thread_items {
    print "datastore.process_thread_items - start\n";
    
    # Find the index of the first "ready item"
    while( 1 ) {
        my $acted = 0;
        for( my $i=0;$i<scalar(@threadItems);$i++ ) {
            my $rawReq = $threadItems[ $i ];
            #if( $ob->{'ready'} ) {
                process_thread_item( $rawReq );
                $acted = 1;
                splice @threadItems, $i, 1;
                last;
            #}
        }
        last if( !$acted );
    }
    
    print "datastore.process_thread_items - end\n";
}

sub process_thread_item {
    my $rawReq = shift;
    print "datastore.process_thread_item - rawReq:\n" . $rawReq . "\n";
    my $ob = Parse::XJR->new( text => $rawReq );
    my $req = $ob->firstChild();
    my $type = $req->name();
    if( $type eq 'teval' ) { type_teval( $req ); }
}

sub handle_datastore_item {
    my ( $buffer, $size ) = @_;
    print "Datastore Received '$buffer'\n\n";
    my $copy = "$buffer";
    my $root = Parse::XJR->new( text => $copy );
    my $req = $root->firstChild();
    my $type = $req->name();
    
    print "  Type=$type\n";
    if(    $type eq 'new_item'       ) { return type_new_item( $req ); }
    elsif( $type eq 'get_agent_addr' ) { return type_get_agent_addr( $req ); }
    elsif( $type eq 'add_agent'      ) { return type_add_agent( $req ); }
    elsif( $type eq 'list_agents'    ) { return type_list_agents( $req ); }
    elsif( $type eq 'result'         ) { return type_result( $req ); }
    elsif( $type eq 'teval'          ) {
        return enqueue_thread_item( $buffer );
    }
    return "unknown type $type";
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
        if( @threadItems ) {
            process_thread_items();
        }
        
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