#!/usr/bin/perl -w
# $Id$

use strict;

use IO::File;
use POSIX;

my $HEADER = <<EOF;
/*  \$Id\$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author\'s official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Aaron Ucko (via $0)
*
* File Description:
*   Protein alignment score matrices; shared between the two toolkits.
*
* ===========================================================================
*/

#include <util/tables/raw_scoremat.h>

EOF

foreach my $filename (@ARGV) {
    my $in = new IO::File($filename);
    if ( !$in ) {
	warn "Unable to open $filename: $!";
	next;
    }
    my $varbase = $filename;
    $varbase =~ s:.*/::;
    $varbase =~ s/([A-Z])([A-Z]+)/$1\L$2/g;
    my $outfn = "sm_\L$varbase.c";
    my $out = new IO::File(">$outfn");
    if ( !$out ) {
	warn "Unable to open $outfn: $!";
	next;
    }
    print $out $HEADER;
    my @symbols;
    my $i;
    my $n;
    my $width; # score entries per line
    my $min;
    while (<$in>) {
	if (s/\# *(.*)//  &&  $1) {
	    print $out '/* ', $1, " */\n";
	}
	my @elts = split;
	next unless @elts;
	if (defined @symbols  &&  @symbols) {
	    if ($elts[0] ne $symbols[$i]) {
		warn "$filename:$.: Expected $symbols[$i] but got $elts[0]";
	    }
	    print $out "    /*$elts[0]*/ {";
	    for (my $j = 0;  $j < $n;  ++$j) {
		if ($j > 0  &&  !($j % $width)) {
		    print $out "\n", ' ' x 11;
		}
		printf $out '%3d', $elts[$j+1];
		if ( !defined($min)  ||  $min > $elts[$j+1]) {
		    $min = $elts[$j+1];
		}
		if ($j == $n - 1) {
		    print $out ' }';
		    print $out ',' unless $i == $n - 1;
		    print $out "\n";
		} else {
		    print $out ',';
		}
	    }
	    ++$i;
	} else {
	    @symbols = @elts;
	    $n = @symbols;
	    $i = 0;
	    print $out
		"\nstatic const TNCBIScore s_${varbase}PSM[$n][$n] = {\n";
	    my $rows = POSIX::ceil($n / 16);
	    # Find the minimum width that yields the necessary number of rows.
	    $width = POSIX::ceil($n / $rows);
	    print $out '    /*     ';
	    for (my $j = 0;  $j < $n;  ++$j) {
		if ($j > 0  &&  !($j % $width)) {
		    print $out "\n", ' ' x 11;
		}
		print $out '  ', $symbols[$j];
		if ($j == $n - 1) {
		    print $out " */\n";
		} else {
		    print $out ',';
		}
	    }
	}
    }
    my $symstr = join '', @symbols;
    print $out <<EOF;
};
const SNCBIPackedScoreMatrix NCBISM_$varbase = {
    "$symstr",
    s_${varbase}PSM[0],
    $min
};
EOF
}
