#!/usr/bin/perl -w

# Copyright (C) SUSE LLC 2019
# Based loosely on https://github.com/nanoscopic/lumith/blob/master/source_lumith/NanoRequest.pm

use strict;
use NanoMsg::Raw;
use Time::HiRes qw/gettimeofday tv_interval/;

my $pid = fork();
if( $pid == 0 ) {
   bind_reqrep();
   exit(0);
}
bind_listen();
exit(0);

sub bind_reqrep {
    my $socket_address_in = "tcp://127.0.0.1:8275";
    umask(0);
    my $socket_in = nn_socket(AF_SP, NN_REP);
    print "Listening for requests on $socket_address_in\n";
    my $bindok = nn_bind($socket_in, $socket_address_in);
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
            $err = decode_err( $err );
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
        
        logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}
sub bind_listen {
    my $socket_address_in = "tcp://127.0.0.1:8274";
    umask(0);
    my $socket_in = nn_socket(AF_SP, NN_PULL);
    print "Listening for results on $socket_address_in\n";
    my $bindok = nn_bind($socket_in, $socket_address_in);
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
            $err = decode_err( $err );
            print "fail to recv: $err\n";
            next;
        }
        my $startTime = [ gettimeofday() ];
        my $res =handle_data( $buf, $bytes );
        my $endTime = [ gettimeofday() ];
        my $len = int( tv_interval( $startTime, $endTime ) * 10000 ) / 10;
        my $sent_bytes = $res->{'sent_bytes'};
        
        logr( type => "visit", time => "${len}ms", sent => "$sent_bytes" );
    }
}

sub logr {
}

sub handle_data_req {
    my ( $buffer, $bytes ) = @_;
    print "\nReceived $buffer\n";
    if( $buffer ) {
        
    }
    return { response  => "test" };
}

sub handle_data {
    my ( $buffer, $bytes ) = @_;
    return { sent_bytes => 0 };
}

sub decode_err {
    my $n = shift;
    return "EBADF" if( $n == EBADF );
    return "EMFILE" if( $n == EMFILE );
    return "EINVAL" if( $n == EINVAL );
    return "ENAMETOOLONG" if( $n == ENAMETOOLONG );
    return "EPROTONOSUPPORT" if( $n == EPROTONOSUPPORT );
    return "EADDRNOTAVAIL" if( $n == EADDRNOTAVAIL );
    return "ENODEV" if( $n == ENODEV );
    return "EADDRINUSE" if( $n == EADDRINUSE );
    return "ETERM" if( $n == ETERM );
    return "ETIMEDOUT" if( $n == ETIMEDOUT );
    return "unknown error - $n";
}
