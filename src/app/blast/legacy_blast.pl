#! /usr/bin/perl -w
# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author:  Christiam Camacho
#
# File Description:
#   Script to convert NCBI C toolkit command line program and arguments into 
#   NCBI C++ toolkit command line program and arguments for the BLAST suite of
#   programs
#
# ===========================================================================

use strict;
use warnings;
use Getopt::Long qw(:config no_ignore_case bundling no_auto_abbrev);
use Pod::Usage;

use constant VERSION => 1.0;

pod2usage({-exitval => 1, -verbose => 2}) if (@ARGV == 0);

my $application = shift;
my $print_only = "1"; # Determines whether script prints or runs the command

my $cmd;
if ($application eq "blastall") {
    $cmd = &handle_blastall(\$print_only);
} elsif ($application eq "megablast") {
    $cmd = &handle_megablast(\$print_only);
} else {
    die "$application is not supported\n";
}

if ($print_only) {
    print "$cmd\n";
} else {
    system($cmd);
}

sub handle_blastall($)
{
    my $print_only = shift;
    my ($opt_A, $opt_B, $opt_C, $opt_D, $opt_E, $opt_F, $opt_G, $opt_I, $opt_J, 
        $opt_K, $opt_L, $opt_M, $opt_O, $opt_P, $opt_Q, $opt_R, $opt_S, $opt_T, 
        $opt_U, $opt_V, $opt_W, $opt_X, $opt_Y, $opt_Z, $opt_a, $opt_b, $opt_d, 
        $opt_e, $opt_f, $opt_g, $opt_i, $opt_l, $opt_m, $opt_n, $opt_o, $opt_p, 
        $opt_q, $opt_r, $opt_s, $opt_t, $opt_v, $opt_w, $opt_y, $opt_z);

    GetOptions("<>"             => sub { $application = shift; },
               "print_only!"    => $print_only,
               "A=i"            => \$opt_A,
               "B=i"            => \$opt_B,
               "C=s"            => \$opt_C,
               "D=i"            => \$opt_D,
               "E=i"            => \$opt_E,
               "F=s"            => \$opt_F,
               "G=i"            => \$opt_G,
               "I=s"            => \$opt_I,
               "J=s"            => \$opt_J,
               "K=i"            => \$opt_K,
               "L=s"            => \$opt_L,
               "M=s"            => \$opt_M,
               "O=s"            => \$opt_O,
               "P=i"            => \$opt_P,
               "Q=i"            => \$opt_Q,
               "R=s"            => \$opt_R,
               "S=i"            => \$opt_S,
               "T=s"            => \$opt_T,
               "U=s"            => \$opt_U,
               "V=s"            => \$opt_V,
               "W=i"            => \$opt_W,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "Z=i"            => \$opt_Z,
               "a=i"            => \$opt_a,
               "b=i"            => \$opt_b,
               "d=s"            => \$opt_d,
               "e=f"            => \$opt_e,
               "f=i"            => \$opt_f,
               "g=s"            => \$opt_g,
               "i=s"            => \$opt_i,
               "l=s"            => \$opt_l,
               "m=i"            => \$opt_m,
               "n=s"            => \$opt_n,
               "o=s"            => \$opt_o,
               "p=s"            => \$opt_p,
               "q=i"            => \$opt_q,
               "r=i"            => \$opt_r,
               "s=s"            => \$opt_s,
               "t=i"            => \$opt_t,
               "v=i"            => \$opt_v,
               "w=i"            => \$opt_w,
               "y=f"            => \$opt_y,
               "z=f"            => \$opt_z,
               );


    if (defined $opt_n and $opt_n =~ /t/i) {
        return &handle_megablast($print_only);
    }

    my $retval;
    $retval .= "./$opt_p "                  if (defined $opt_p);
    $retval .= "-db $opt_d "                if (defined $opt_d);
    $retval .= "-query $opt_i "             if (defined $opt_i);
    $retval .= "-gilist $opt_l "            if (defined $opt_l);
    $retval .= "-dbsize $opt_z "            if (defined $opt_z);
    $retval .= "-matrix $opt_M "            if (defined $opt_M);
    $retval .= "-evalue $opt_e "            if (defined $opt_e);
    $retval .= "-gapopen $opt_G "           if (defined $opt_G);
    $retval .= "-gapextend $opt_E "         if (defined $opt_E);
    $retval .= "-xdrop_ungap $opt_y "       if (defined $opt_y);
    $retval .= "-xdrop_gap $opt_X "         if (defined $opt_X);
    $retval .= "-xdrop_gap_final $opt_Z "   if (defined $opt_Z);
    $retval .= "-num_threads $opt_a "       if (defined $opt_a);
    $retval .= "-window_size $opt_A "       if (defined $opt_A);
    $retval .= "-word_size $opt_W "         if (defined $opt_W);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    $retval .= "-min_word_score $opt_f "    if (defined $opt_f);
    if (defined $opt_I and $opt_I =~ /t/i) {
        $retval .= "-show_gis ";
    }
    $retval .= "-num_descriptions $opt_v "  if (defined $opt_v);
    $retval .= "-num_alignments $opt_b "    if (defined $opt_b);
    $retval .= "-query_gencode $opt_Q "     if (defined $opt_Q);
    $retval .= "-db_gencode $opt_D "        if (defined $opt_D);
    $retval .= "-mismatch_penalty $opt_q "  if (defined $opt_q);
    $retval .= "-match_reward $opt_r "      if (defined $opt_r);
    $retval .= "-culling_limit $opt_K "     if (defined $opt_K);
    $retval .= "-max_intron_length $opt_t " if (defined $opt_t);
    $retval .= "-frame_shift_penalty $opt_w " if (defined $opt_w);
    if (defined $opt_C) {
        # handle composition based statistics!
        $retval .= "-comp_based_stats $opt_C ";
    }

    $retval .= "-out $opt_o "               if (defined $opt_o);
    $retval .= "-outfmt $opt_m "            if (defined $opt_m);
    if (defined $opt_O) {
        unless ($retval =~ s/-out \S+ /-out $opt_O /) {
            $retval .= "-out $opt_O ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 10/) {
            $retval .= "-outfmt 10 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }

    if (defined $opt_L) {
        $retval .= "-query_loc ";
        my @fields = split(/[ ;,]/, $opt_L);
        $retval .= "$fields[0]-$fields[1] ";
    }
    if (defined $opt_U and $opt_U =~ /t/i) {
        $retval .= "-lcase_masking ";
    }
    if (defined $opt_g and $opt_g =~ /f/i) {
        $retval .= "-ungapped ";
    }
    if (defined $opt_J and $opt_J =~ /t/i) {
        $retval .= "-parse_query_defline ";
    }
    if (defined $opt_S) {
        $retval .= "-strand ";
        if ($opt_S == 1) {
            $retval .= "plus ";
        } elsif ($opt_S == 2) {
            $retval .= "minus ";
        } else {
            $retval .= "both ";
        }
    }
    if (defined $opt_s and $opt_s =~ /t/i) {
        $retval .= "-use_sw_tback ";
    }


    return $retval;
}

sub handle_megablast($)
{
    my $print_only = shift;
    my $retval;

    return $retval;
}

sub handle_blastpgp($)
{
    my $print_only = shift;
    my $retval;

    $retval .= "-gap_trigger $opt_N "       if (defined $opt_N);
    $retval .= "-num_iterations $opt_j "    if (defined $opt_j);
    $retval .= "-pseudocount $opt_c "       if (defined $opt_c);
    $retval .= "-inclusion_ethresh $opt_h " if (defined $opt_h);
    # Will these work ?
    $retval .= "-in_pssm $opt_R "           if (defined $opt_R);
    $retval .= "-out_pssm $opt_C "          if (defined $opt_C);

    return $retval;
}
__END__

=head1 NAME

B<legacy_blast.pl> - Convert 

=head1 SYNOPSIS

legacy_blast.pl [options] blastdb ...

=head1 OPTIONS

=over 2

=item B<--showall>

Show all available pre-formatted BLAST databases (default: false). The output
of this option lists the database names which should be used when
requesting downloads or updates using this script.

=item B<--passive>

Use passive FTP, useful when behind a firewall (default: false).

=item B<--timeout>

Timeout on connection to NCBI (default: 120 seconds).

=item B<--force>

Force download even if there is a archive already on local directory (default:
false).

=item B<--verbose>

Increment verbosity level (default: 1). Repeat this option multiple times to 
increase the verbosity level (maximum 2).

=item B<--quiet>

Produce no output (default: false). Overrides the B<--verbose> option.

=back

=head1 DESCRIPTION

This script will download the pre-formatted BLAST databases requested in the
command line from the NCBI ftp site.

=head1 EXIT CODES

This script returns 0 on success and a non-zero value on errors.

=head1 BUGS

Please report them to <blast-help@ncbi.nlm.nih.gov>

=head1 COPYRIGHT

See PUBLIC DOMAIN NOTICE included at the top of this script.

=cut
