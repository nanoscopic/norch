package part::misc;
use strict;
use warnings;
use NanoMsg::Raw;

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

1;