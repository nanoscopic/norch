#!/usr/bin/perl -w
use strict;
use NanoMsg::Raw;
use Parse::XJR;

my $socket_out = connect_director();
my $out = $ARGV[0];
#print "Sending $out\n";
my $response = handle_cmdline( $socket_out, $out );
print $response;

sub connect_director {
    my $socket_address_out = "tcp://127.0.0.1:8275";
    umask(0);
    my $socket_out = nn_socket(AF_SP, NN_REQ);
    print "Listening on $socket_address_out\n";
    my $bindok = nn_connect($socket_out, $socket_address_out);
    if( !$bindok ) {
        my $err = nn_errno();
        $err = decode_err( $err );
        die "fail to connect: $err";
    }
    nn_setsockopt( $socket_out, NN_SOL_SOCKET, NN_RCVTIMEO, 2000 ); # timeout receive in 2000ms
    return $socket_out;
}

sub handle_item {
    my ( $socket, $item ) = @_;
    
    my $type = $item->name();
    
    my %passThruTypes = (
        add_agent => 1,
        list_agents => 1,
        cmd => 1
    );
    
    if( $passThruTypes{ $type } ) {
        my $xjr = "<$type>" . $item->xjr() . "</$type>";
        nn_send( $socket, $xjr, 0 );
        my $buffer;
        nn_recv( $socket, $buffer, 1000, 0 );
        return $buffer;
    }
    elsif( $type eq 'inorder' ) {
        # assign random names to each item
        # and make them dependent one after another
        # then run handle_item on each
        
        my $du = Data::UUID->new;
        my $curChild = $item->firstChild();
        my $prevName;
        while( $curChild ) {
            if( !$curChild->isatt() ) { # Ignore attributes of the inorder tag itself
                if( $prevName ) {
                    # The items themselves could already have dependencies
                    # TODO: tack on to existing dependencies
                    $curChild->{dep} = $prevName;
                }
                $prevName = $curChild->{name} = $du->create();
            }
            
            $curChild = $curChild->next();
        }
        my $lastChild = $curChild; # for sanity
        if( $item->{name} ) { # Let a while set of things done in order in turn trigger other items
            my $setName = $item->{name};
            $lastChild->{name} = $setName;
        }
        
        return handle_items( $socket, $item );
    }
    else {
        return "Unknown request type $type";
    }
}

sub handle_items {
    my ( $socket, $root ) = @_;
    
    my $results = '';
    my $curChild = $root->firstChild();
    while( $curChild ) {
        $results .= handle_item( $socket, $curChild ) . "\n";
        $curChild = $curChild->next();
    }
    return $results;
}

sub handle_cmdline {
    my ( $socket, $req ) = @_;
    
    if( $req eq 'xjr' ) {
        my $xjrFile = $ARGV[1];
        
        my $root = Parse::XJR->new( file => $xjrFile );
        return handle_items( $socket, $root );
    }
    
    my $msg;
    if( $req eq 'cmd' ) {
        my $cmd = $ARGV[1];
        $msg = "<cmd><cmd><![CDATA[$cmd]]></cmd></cmd>";
    }
    else {    
        $msg = "<$req/>";
    }
    
    nn_send( $socket, $msg, 0 );

    my $buffer;
    
    nn_recv( $socket, $buffer, 100, 0 );
    
    # Just returning the raw response currently until we decide how to display it cleaner
    return $buffer;
}