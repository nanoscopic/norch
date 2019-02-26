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
        #if( $err == EADDRINUSE ) {
        #    my $connectok = nn_connect($socket_in, $socket_address_in);
        #    if( !$connectok ) {
        #        die "could not connect after fail on bind";
        #    }
        #}
        #else {
            $err = decode_err( $err );
            die "fail to connect: $err";
        #}
    }
    nn_setsockopt( $socket_out, NN_SOL_SOCKET, NN_RCVTIMEO, 2000 ); # timeout receive in 2000ms
    return $socket_out;
}

sub handle_request {
    my ( $socket, $req ) = @_;
    
    my $type = $req->{'type'}->value();
    #my $xjr = $req->xjr();
    my $xjr;
    if   ( $type eq 'addnode'   ) { $xjr = "<req>". $req->xjr() . "</req>"; }
    elsif( $type eq 'listnodes' ) { $xjr = "<req>". $req->xjr() . "</req>"; }
    elsif( $type eq 'cmd'       ) { $xjr = "<req>". $req->xjr() . "</req>"; }
    if( $xjr ) {
        nn_send( $socket, $xjr, 0 );
        my $buffer;
        nn_recv( $socket, $buffer, 1000, 0 );
        
        my $res = Parse::XJR->new( text => $buffer );
        my $resText = $res->{result}->value();
        
        return $resText;
    }
    else {
        return "Unknown request type $type";
    }
}

sub handle_cmdline {
    my ( $socket, $req ) = @_;
    
    my $msg;
    if( $req eq 'cmd' ) {
        my $cmd = $ARGV[1];
        $msg = "<req type='$req'><cmd><![CDATA[$cmd]]></cmd></req>";
    }
    elsif( $req eq 'xjr' ) {
        my $xjrFile = $ARGV[1];
        my $results = '';
        my $root = Parse::XJR->new( file => $xjrFile );
        my $reqs = $root->{'@req'};
        for my $req ( @$reqs ) {
            $results .= handle_request( $socket, $req ) . "\n";
        }
        return $results;
    }
    else {    
        $msg = "<req type='$req'/>";
    }
    
    nn_send( $socket, $msg, 0 );

    my $buffer;
    
    nn_recv( $socket, $buffer, 100, 0 );
    
    my $res = Parse::XJR->new( text => $buffer );
    my $resText = $res->{result}->value();
    
    return $resText;
}