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
use constant DEBUG => 1;

pod2usage({-exitval => 1, -verbose => 2}) if (@ARGV == 0);

my $application = shift;
my $print_only = "0"; # Determines whether script prints or runs the command

my $cmd;
if ($application eq "blastall") {
    $cmd = &handle_blastall(\$print_only);
} elsif ($application eq "megablast") {
    $cmd = &handle_megablast(\$print_only);
} elsif ($application eq "blastpgp") {
    $cmd = &handle_blastpgp(\$print_only);
} elsif ($application eq "bl2seq") {
    $cmd = &handle_bl2seq(\$print_only);
} elsif ($application eq "rpsblast") {
    $cmd = &handle_rpsblast(\$print_only);
} else {
    die "Application: '$application' is not supported\n";
}

if ($print_only) {
    print "$cmd\n";
} else {
    print STDERR "$cmd\n" if (DEBUG);
    system($cmd);
}

sub convert_sequence_locations($$)
{
    my $arg = shift;
    my $target = shift;
    my $retval;
    if (defined $arg) {
        if ($target eq "query") {
            $retval .= "-query_loc ";
        } else {
            $retval .= "-subject_loc ";
        }
        my @fields = split(/[ ;,]/, $arg);
        $retval .= "$fields[0]-$fields[1] ";
    }
    return $retval;
}

sub convert_filter_string($$)
{
    use constant DUST_ARGS => "\"20 64 1\"";
    use constant SEG_ARGS => "\"12 2.2 2.5\"";
    my $filter_string = shift;
    my $program = shift;

    #print STDERR "Parsing '$filter_string'\n";

    if ($filter_string =~ /F/) {
        if ($program eq "blastp" or $program eq "tblastn") {
            return "-seg no";
        } else {
            return "-dust no";
        }
    }

    my $retval = "";
    if ($filter_string =~ /S (\d+) (\S+) (\S+)/) {
        $retval .= "-seg '$1 $2 $3' ";
    }
    if ($filter_string =~ /D (\d+) (\d+) (\d+)/) {
        $retval .= "-dust '$1 $2 $3' ";
    }
    if ($filter_string =~ /R -d (\S+)/) {
        $retval .= "-filtering_db $1 ";
    } elsif ($filter_string =~ /R\s*;/) {
        $retval .= "-filtering_db repeat/repeat_9606 ";
    }

    if ($filter_string =~ /L|T/) {
        if ($program eq "blastp" or $program eq "tblastn") {
            $retval .= "-seg " . SEG_ARGS . " ";
        } else {
            $retval .= "-dust " . DUST_ARGS . " ";
        }
    }

    if ($filter_string =~ /m/) {
        $retval .= "-soft_masking true ";
    }
    #print STDERR "returning '$retval'\n";
    return $retval;
}

sub convert_strand($)
{
    my $old_strand_arg = shift;
    my $retval = "-strand ";
    if ($old_strand_arg == 1) {
        $retval .= "plus ";
    } elsif ($old_strand_arg == 2) {
        $retval .= "minus ";
    } else {
        $retval .= "both ";
    }
    return $retval;
}

# Handle the conversion from blastall arguments to the corresponding C++
# binaries
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
               "B=i"            => \$opt_B, # not handled, not applicable
               "C=s"            => \$opt_C,
               "D=i"            => \$opt_D,
               "E=i"            => \$opt_E,
               "F=s"            => \$opt_F,
               "G=i"            => \$opt_G,
               "I:s"            => \$opt_I,
               "J:s"            => \$opt_J,
               "K=i"            => \$opt_K,
               "L=s"            => \$opt_L,
               "M=s"            => \$opt_M,
               "O=s"            => \$opt_O,
               "P=i"            => \$opt_P,
               "Q=i"            => \$opt_Q,
               "R=s"            => \$opt_R,
               "S=i"            => \$opt_S,
               "T:s"            => \$opt_T,
               "U:s"            => \$opt_U,
               "V:s"            => \$opt_V, # not handled, not applicable
               "W=i"            => \$opt_W,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "Z=i"            => \$opt_Z,
               "a=i"            => \$opt_a,
               "b=i"            => \$opt_b,
               "d=s"            => \$opt_d,
               "e=f"            => \$opt_e,
               "f=i"            => \$opt_f,
               "g:s"            => \$opt_g,
               "i=s"            => \$opt_i,
               "l=s"            => \$opt_l,
               "m=i"            => \$opt_m,
               "n:s"            => \$opt_n,
               "o=s"            => \$opt_o,
               "p=s"            => \$opt_p,
               "q=i"            => \$opt_q,
               "r=i"            => \$opt_r,
               "s:s"            => \$opt_s,
               "t=i"            => \$opt_t,
               "v=i"            => \$opt_v,
               "w=i"            => \$opt_w,
               "y=f"            => \$opt_y,
               "z=f"            => \$opt_z,
               );

    unless (defined $opt_p) {
        die "-p must be provided\n";
    }

    my $retval;
    if (defined $opt_p) {
        if (defined $opt_R) {
            $retval .= "./tblastn -in_pssm $opt_R ";
        } elsif (defined $opt_n and $opt_n =~ /t/i) {
            $retval .= "./blastn -task megablast ";
        } else {
            $retval .= "./$opt_p ";                 
            $retval .= "-task blastn " if ($opt_p eq "blastn");
        }
    }
    $retval .= "-db \"$opt_d\" "            if (defined $opt_d);
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
    if (defined $opt_A) {
        if (defined $opt_P and $opt_P ne "0") {
            print STDERR "Warning: ignoring -P because window size is set\n";
        }
        $retval .= "-window_size $opt_A "       
    }
    if (defined $opt_P and $opt_P eq "1" and (not defined $opt_A)) {
        $retval .= "-window_size 0 ";
    }
    $retval .= "-word_size $opt_W "         if (defined $opt_W);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    if (defined $opt_f) {
        unless ($opt_p eq "blastn") {
            $retval .= "-min_word_score $opt_f "    
        } else {
            print STDERR "Warning: -f is not supported for blastn\n";
        }
    }
    if (defined $opt_I and (length($opt_I) == 0 or $opt_I =~ /t/i)) {
        $retval .= "-show_gis ";
    }
    $retval .= "-num_descriptions $opt_v "  if (defined $opt_v);
    $retval .= "-num_alignments $opt_b "    if (defined $opt_b);
    $retval .= "-query_gencode $opt_Q "     if (defined $opt_Q);
    $retval .= "-db_gencode $opt_D "        if (defined $opt_D);
    $retval .= "-penalty $opt_q "           if (defined $opt_q);
    $retval .= "-reward $opt_r "            if (defined $opt_r);
    $retval .= "-culling_limit $opt_K "     if (defined $opt_K);
    $retval .= "-max_intron_length $opt_t " if (defined $opt_t);
    $retval .= "-frame_shift_penalty $opt_w " if (defined $opt_w);
    if (defined $opt_C) {
        $retval .= "-comp_based_stats $opt_C ";
    } elsif ($opt_p eq "blastp" or $opt_p eq "tblastn") {
        # this is needed b/c blastall -p blastp has CBS turned off by default 
        # while the CBlastAdvancedProteinOptionsHandle has it on by default
        $retval .= "-comp_based_stats F ";
    }

    $retval .= "-out $opt_o "               if (defined $opt_o);
    if (defined $opt_m) {
        if ($opt_m == 5 or $opt_m == 6) {
            print STDERR "Warning: -m5 or -m6 formatting options ";
            print STDERR "are not supported!\n";
        }
        $opt_m -= 2 if ($opt_m >= 7);
        $retval .= "-outfmt $opt_m "            
    }
    if (defined $opt_O) {
        unless ($retval =~ s/-out \S+ /-out $opt_O /) {
            $retval .= "-out $opt_O ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 8/) {
            $retval .= "-outfmt 8 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }
    if (defined $opt_T and (length($opt_T) == 0 or $opt_T =~ /t/i)) {
        $retval .= "-html "                     
    }

    $retval .= &convert_sequence_locations($opt_L, "query") if ($opt_L);
    if (defined $opt_U and (length($opt_U) == 0 or $opt_U =~ /t/i)) {
        $retval .= "-lcase_masking ";
    }
    if (defined $opt_g and $opt_g =~ /f/i) {
        $retval .= "-ungapped ";
    }
    if (defined $opt_J and (length($opt_J) == 0 or $opt_J =~ /t/i)) {
        $retval .= "-parse_query_defline ";
    }
    $retval .= &convert_strand($opt_S) if (defined $opt_S);
    if (defined $opt_s and (length($opt_s) == 0 or $opt_s =~ /t/i)) {
        $retval .= "-use_sw_tback ";
    }

    if (defined $opt_F) {
        $retval .= &convert_filter_string($opt_F, $opt_p);
    }

    return $retval;
}

sub handle_megablast($)
{
    my $print_only = shift;
    my ($opt_A, $opt_D, $opt_E, $opt_F, $opt_G, $opt_H, $opt_I, $opt_J, 
        $opt_L, $opt_M, $opt_N, $opt_O, $opt_P, $opt_Q, $opt_R, $opt_S, 
        $opt_T, $opt_U, $opt_V, $opt_W, $opt_X, $opt_Y, $opt_Z, $opt_a, 
        $opt_b, $opt_d, $opt_e, $opt_f, $opt_g, $opt_i, $opt_l, $opt_m, 
        $opt_n, $opt_o, $opt_p, $opt_q, $opt_r, $opt_s, $opt_t, $opt_v, 
        $opt_y, $opt_z);

    GetOptions("<>"             => sub { $application = shift; },
               "print_only!"    => $print_only,
               "A=i"            => \$opt_A,
               "D=i"            => \$opt_D,
               "E=i"            => \$opt_E,
               "F=s"            => \$opt_F,
               "G=i"            => \$opt_G,
               "H=i"            => \$opt_H,
               "I:s"            => \$opt_I,
               "J:s"            => \$opt_J,
               "L=s"            => \$opt_L,
               "M=i"            => \$opt_M,
               "N=i"            => \$opt_N,
               "O=s"            => \$opt_O,
               "P=i"            => \$opt_P, # no equivalent in new engine
               "Q=s"            => \$opt_Q,
               "R:s"            => \$opt_R,
               "S=i"            => \$opt_S,
               "T:s"            => \$opt_T,
               "U:s"            => \$opt_U,
               "V:s"            => \$opt_V, # not handled, not applicable
               "W=i"            => \$opt_W,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "Z=i"            => \$opt_Z,
               "a=i"            => \$opt_a,
               "b=i"            => \$opt_b,
               "d=s"            => \$opt_d,
               "e=f"            => \$opt_e,
               "f:s"            => \$opt_f,
               "g:s"            => \$opt_g,
               "i=s"            => \$opt_i,
               "l=s"            => \$opt_l,
               "m=i"            => \$opt_m,
               "n:s"            => \$opt_n,
               "o=s"            => \$opt_o,
               "p=f"            => \$opt_p,
               "q=i"            => \$opt_q,
               "r=i"            => \$opt_r,
               "s=i"            => \$opt_s,
               "t=i"            => \$opt_t,
               "v=i"            => \$opt_v,
               "y=i"            => \$opt_y,
               "z=f"            => \$opt_z
               );
    my $retval;

    $retval = "./blastn ";
    $retval .= "-query $opt_i "             if (defined $opt_i);
    $retval .= "-db \"$opt_d\" "            if (defined $opt_d);
    $retval .= "-evalue $opt_e "            if (defined $opt_e);
    $retval .= "-xdrop_gap $opt_X "         if (defined $opt_X);
    $retval .= "-gilist $opt_l "            if (defined $opt_l);
    $retval .= "-penalty $opt_q "           if (defined $opt_q);
    $retval .= "-reward $opt_r "            if (defined $opt_r);
    $retval .= "-gapopen $opt_G "           if (defined $opt_G);
    $retval .= "-gapextend $opt_E "         if (defined $opt_E);
    $retval .= "-out $opt_o "               if (defined $opt_o);
    if (defined $opt_m) {
        if ($opt_m == 5 or $opt_m == 6) {
            print STDERR "Warning: -m5 or -m6 formatting options ";
            print STDERR "are not supported!\n";
        }
        $opt_m -= 2 if ($opt_m >= 7);
        $retval .= "-outfmt $opt_m "            
    }
    if (defined $opt_O) {
        unless ($retval =~ s/-out \S+ /-out $opt_O /) {
            $retval .= "-out $opt_O ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 8/) {
            $retval .= "-outfmt 8 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }
    if (defined $opt_T and (length($opt_T) == 0 or $opt_T =~ /t/i)) {
        $retval .= "-html "                     
    }
    $retval .= "-num_descriptions $opt_v "  if (defined $opt_v);
    $retval .= "-num_alignments $opt_b "    if (defined $opt_b);
    $retval .= "-num_threads $opt_a "       if (defined $opt_a);
    $retval .= "-word_size $opt_W "         if (defined $opt_W);
    $retval .= "-dbsize $opt_z "            if (defined $opt_z);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    $retval .= "-xdrop_ungap $opt_y "       if (defined $opt_y);
    $retval .= "-xdrop_gap_final $opt_Z "   if (defined $opt_Z);
    $retval .= "-template_length $opt_t "   if (defined $opt_t);
    $retval .= "-window_size $opt_A "       if (defined $opt_A);
    if (defined $opt_N) {
        $retval .= "-template_type coding " if ($opt_N == 0);
        $retval .= "-template_type optimal " if ($opt_N == 1);
        $retval .= "-template_type coding_and_optimal " if ($opt_N == 2);
    }
    if (defined $opt_F) {
        $retval .= &convert_filter_string($opt_F, "blastn");
    }
    if (defined $opt_I and (length($opt_I) == 0 or $opt_I =~ /t/i)) {
        $retval .= "-show_gis ";
    }
    if (defined $opt_J and (length($opt_J) == 0 or $opt_J =~ /t/i)) {
        $retval .= "-parse_query_defline ";
    }


    $retval .= "-perc_identity $opt_p " if (defined $opt_p);
    $retval .= "-min_raw_gapped_score $opt_s " if (defined $opt_s);
    $retval .= &convert_strand($opt_S) if (defined $opt_S);
    $retval .= &convert_sequence_locations($opt_L, "query") if ($opt_L);
    if (defined $opt_U and (length($opt_U) == 0 or $opt_U =~ /t/i)) {
        $retval .= "-lcase_masking ";
    }
    if (defined $opt_n and (length($opt_n) == 0 or $opt_n =~ /t/i)) {
        $retval .= "-no_greedy ";
    }

    # Unsupported options
    if (defined $opt_M) {
        print STDERR "Warning: -M option is ignored\n";
    }
    if (defined $opt_D) {
        print STDERR "Warning: -D option is not supported!\n";
    }
    if (defined $opt_g and $opt_g =~ /f/i) {
        print STDERR "Warning: -g option is not supported!\n";
    }
    if (defined $opt_H) {
        print STDERR "Warning -H option is not supported!\n";
    }

    # Deprecated options
    if (defined $opt_f) {
        print STDERR "Warning: -f option is deprecated\n";
    }
    if (defined $opt_R) {
        print STDERR "Warning: -R option is deprecated\n";
    }
    if (defined $opt_Q) {
        print STDERR "Warning: -Q option is deprecated\n";
    }
    if (defined $opt_P) {
        print STDERR "Warning: -P option is deprecated\n";
    }

    return $retval;
}

sub handle_blastpgp($)
{
    my $print_only = shift;
    my ($opt_A, $opt_B, $opt_C, $opt_E, $opt_F, $opt_G, $opt_H, $opt_I, 
        $opt_J, $opt_K, $opt_L, $opt_M, $opt_N, $opt_O, $opt_P, $opt_Q, 
        $opt_R, $opt_S, $opt_T, $opt_U, $opt_W, $opt_X, $opt_Y, $opt_Z, 
        $opt_a, $opt_b, $opt_c, $opt_d, $opt_e, $opt_f, $opt_h, $opt_i, 
        $opt_j, $opt_k, $opt_l, $opt_m, $opt_o, $opt_p, $opt_q, $opt_s, 
        $opt_t, $opt_u, $opt_v, $opt_y, $opt_z);

    GetOptions("<>"             => sub { $application = shift; },
               "print_only!"    => $print_only,
               "A=i"            => \$opt_A,
               "B=s"            => \$opt_B,
               "C=s"            => \$opt_C,
               "E=i"            => \$opt_E,
               "F=s"            => \$opt_F,
               "G=i"            => \$opt_G,
               "H=i"            => \$opt_H,
               "I:s"            => \$opt_I,
               "J:s"            => \$opt_J,
               "K=i"            => \$opt_K,
               "L=i"            => \$opt_L,
               "M=s"            => \$opt_M,
               "N=f"            => \$opt_N,
               "O=s"            => \$opt_O,
               "P=i"            => \$opt_P,
               "Q=s"            => \$opt_Q,
               "R=s"            => \$opt_R,
               "S=i"            => \$opt_S,
               "T:s"            => \$opt_T,
               "U:s"            => \$opt_U,
               "W=i"            => \$opt_W,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "Z=i"            => \$opt_Z,
               "a=i"            => \$opt_a,
               "b=i"            => \$opt_b,
               "c=i"            => \$opt_c,
               "d=s"            => \$opt_d,
               "e=f"            => \$opt_e,
               "f=i"            => \$opt_f,
               "h=f"            => \$opt_h,
               "i=s"            => \$opt_i,
               "j=i"            => \$opt_j,
               "k=s"            => \$opt_k,
               "l=s"            => \$opt_l,
               "m=i"            => \$opt_m,
               "o=s"            => \$opt_o,
               "p=s"            => \$opt_p,
               "q=i"            => \$opt_q,
               "s:s"            => \$opt_s,
               "t=s"            => \$opt_t,
               "u=i"            => \$opt_u,
               "v=i"            => \$opt_v,
               "y=f"            => \$opt_y,
               "z=f"            => \$opt_z
               );
    #FIXME: should this binary be renamed to something like: iterated_blast
    my $retval = "./psiblast ";

    my $query_is_protein = "1";

    if (defined $opt_p and ($opt_p ne "blastpgp" or
                            $opt_p ne "patseedp")) {
        die "Program '$opt_p' not implemented\n";
    }

    $retval .= "-db \"$opt_d\" "            if (defined $opt_d);
    $retval .= "-query $opt_i "             if (defined $opt_i);
    $retval .= "-gilist $opt_l "            if (defined $opt_l);
    $retval .= "-gap_trigger $opt_N "       if (defined $opt_N);
    $retval .= "-matrix $opt_M "            if (defined $opt_M);
    $retval .= "-num_iterations $opt_j "    if (defined $opt_j);
    $retval .= "-min_word_score $opt_f "    if (defined $opt_f);
    $retval .= "-evalue $opt_e "            if (defined $opt_e);
    $retval .= "-gapopen $opt_G "           if (defined $opt_G);
    $retval .= "-gapextend $opt_E "         if (defined $opt_E);
    $retval .= "-num_threads $opt_a "       if (defined $opt_a);
    $retval .= "-dbsize $opt_z "            if (defined $opt_z);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    $retval .= "-pseudocount $opt_c "       if (defined $opt_c);
    $retval .= "-inclusion_ethresh $opt_h " if (defined $opt_h);
    if (defined $opt_A) {
        if (defined $opt_P and $opt_P ne "0") {
            print STDERR "Warning: ignoring -P because window size is set\n";
        }
        $retval .= "-window_size $opt_A "       
    }
    if (defined $opt_P and $opt_P eq "1" and (not defined $opt_A)) {
        $retval .= "-window_size 0 ";
    }
    $retval .= "-word_size $opt_W "         if (defined $opt_W);
    $retval .= "-xdrop_ungap $opt_y "       if (defined $opt_y);
    $retval .= "-xdrop_gap $opt_X "         if (defined $opt_X);
    $retval .= "-xdrop_gap_final $opt_Z "   if (defined $opt_Z);
    $retval .= "-num_descriptions $opt_v "  if (defined $opt_v);
    $retval .= "-num_alignments $opt_b "    if (defined $opt_b);
    $retval .= "-culling_limit $opt_K "     if (defined $opt_K);
    $retval .= "-comp_based_stats $opt_t "  if (defined $opt_t);
    $retval .= "-phi_pattern $opt_k "       if (defined $opt_k);
    $retval .= "-out $opt_o "               if (defined $opt_o);
    if (defined $opt_m) {
        if ($opt_m == 5 or $opt_m == 6) {
            print STDERR "Warning: -m5 or -m6 formatting options ";
            print STDERR "are not supported!\n";
        }
        $opt_m -= 2 if ($opt_m >= 7);
        $retval .= "-outfmt $opt_m "            
    }
    if (defined $opt_O) {
        unless ($retval =~ s/-out \S+ /-out $opt_O /) {
            $retval .= "-out $opt_O ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 8/) {
            $retval .= "-outfmt 8 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }
    if (defined $opt_T and (length($opt_T) == 0 or $opt_T =~ /t/i)) {
        $retval .= "-html "                     
    }
    if (defined $opt_I and (length($opt_I) == 0 or $opt_I =~ /t/i)) {
        $retval .= "-show_gis ";
    }
    if (defined $opt_J and (length($opt_J) == 0 or $opt_J =~ /t/i)) {
        $retval .= "-parse_query_defline ";
    }
    if (defined $opt_s and (length($opt_s) == 0 or $opt_s =~ /t/i)) {
        $retval .= "-use_sw_tback ";
    }
    if (defined $opt_U and (length($opt_U) == 0 or $opt_U =~ /t/i)) {
        $retval .= "-lcase_masking ";
    }
    if (defined $opt_F) {
        $retval .= &convert_filter_string($opt_F, 
                                          ($query_is_protein eq "1")
                                          ? "blastp" : "blastn");
    }

    my $location = "";
    $location .= $opt_S if (defined $opt_S);
    if (defined $opt_H) {
        if ($location eq "") {
            $location = "0,$opt_H";
        } else {
            $location .= ",$opt_H";
        }
    }
    if ($location ne "") {
        $location .= ",-1" unless (defined $opt_H);
        $retval .= &convert_sequence_locations($location, "query");
    }

    # Checkpoint file recovery
    if (defined $opt_R) {
        if (defined $opt_q and $opt_q ne "0") {
            $retval .= "-in_pssm $opt_R " 
        } else {
            die "ERROR: recovery from C toolkit checkpoint " .
                "file format not supported\n";
        }
    }

    # Checkpoint file saving
    if (defined $opt_C) {
        if (defined $opt_C and $opt_u ne "0") {
            $retval .= "-out_pssm $opt_C "          
        } else {
            die "ERROR: saving PSSM to C toolkit checkpoint " .
                "file format not supported\n";
        }
    }

    if (defined $opt_Q) {
        print STDERR "Warning: -Q is being ignored\n";
    }
    if (defined $opt_B) {
        print STDERR "Warning: -B is being ignored\n";
    }

    return $retval;
}

# Tested: all conversions should work
sub handle_bl2seq
{
    use File::Temp qw(:POSIX);  # for tmpnam

    my $print_only = shift;
    my ($opt_A, $opt_D, $opt_E, $opt_F, $opt_G, $opt_I, $opt_J, $opt_M, 
        $opt_S, $opt_T, $opt_U, $opt_V, $opt_W, $opt_X, $opt_Y, $opt_a, 
        $opt_d, $opt_e, $opt_g, $opt_i, $opt_j, $opt_m, $opt_o, $opt_p, 
        $opt_q, $opt_r, $opt_t);

    GetOptions("<>"             => sub { $application = shift; },
               "print_only!"    => $print_only,
               "A:s"            => \$opt_A,
               "D=i"            => \$opt_D,
               "E=i"            => \$opt_E,
               "F=s"            => \$opt_F,
               "G=i"            => \$opt_G,
               "I=s"            => \$opt_I,
               "J=s"            => \$opt_J,
               "M=s"            => \$opt_M,
               "S=i"            => \$opt_S,
               "T:s"            => \$opt_T,
               "U:s"            => \$opt_U,
               "V:s"            => \$opt_V, # not handled, not applicable
               "W=i"            => \$opt_W,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "a=s"            => \$opt_a,
               "d=f"            => \$opt_d,
               "e=f"            => \$opt_e,
               "g:s"            => \$opt_g,
               "i=s"            => \$opt_i,
               "j=s"            => \$opt_j,
               "m:s"            => \$opt_m,
               "o=s"            => \$opt_o,
               "p=s"            => \$opt_p,
               "q=i"            => \$opt_q,
               "r=i"            => \$opt_r,
               "t=i"            => \$opt_t
               );
    my $retval;
    my $cmd_prefix = "";
    my $cmd_suffix = "";

    unless (defined $opt_i and defined $opt_j) {
        die "-i and -j are required in bl2seq\n";
    }

    $retval .= "./$opt_p "                  if (defined $opt_p);
    unless (defined $opt_A) {
        $retval .= "-query $opt_i "             if (defined $opt_i);
        $retval .= "-subject $opt_j "           if (defined $opt_j);
    } else {
        # The -A option is not supported, so we create temporary files to
        # simulate it
        my $query_fname = tmpnam();
        my $subj_fname = tmpnam();
        $cmd_prefix = "echo $opt_i > $query_fname; echo $opt_j > $subj_fname; ";
        $cmd_suffix = " ; rm $query_fname $subj_fname;";
        $retval .= "-query $query_fname -subject $subj_fname ";
    }
    $retval .= "-out $opt_o "               if (defined $opt_o);
    if (defined $opt_a) {
        unless ($retval =~ s/-out \S+ /-out $opt_a /) {
            $retval .= "-out $opt_a ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 8/) {
            $retval .= "-outfmt 8 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }
    if (defined $opt_T and (length($opt_T) == 0 or $opt_T =~ /t/i)) {
        $retval .= "-html "                     
    }
    $retval .= "-evalue $opt_e "            if (defined $opt_e);
    $retval .= "-gapopen $opt_G "           if (defined $opt_G);
    $retval .= "-gapextend $opt_E "         if (defined $opt_E);
    $retval .= "-word_size $opt_W "         if (defined $opt_W);
    $retval .= "-matrix $opt_M "            if (defined $opt_M);
    $retval .= "-penalty $opt_q "           if (defined $opt_q);
    $retval .= "-reward $opt_r "            if (defined $opt_r);
    $retval .= &convert_strand($opt_S)      if (defined $opt_S);
    $retval .= "-max_intron_length $opt_t " if (defined $opt_t);
    $retval .= "-dbsize $opt_d "            if (defined $opt_d);
    $retval .= "-xdrop_gap $opt_X "         if (defined $opt_X);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    if (defined $opt_U and (length($opt_U) == 0 or $opt_U =~ /t/i)) {
        $retval .= "-lcase_masking ";
    }
    if (defined $opt_m and (length($opt_m) == 0 or $opt_m =~ /t/i)) {
        $retval .= "-task megablast ";
    }
    if (defined $opt_g and $opt_g =~ /f/i) {
        $retval .= "-ungapped ";
    }
    $retval .= &convert_sequence_locations($opt_I, "query") if ($opt_I);
    $retval .= &convert_sequence_locations($opt_J, "subject") if ($opt_J);

    if (defined $opt_F) {
        $retval .= &convert_filter_string($opt_F, $opt_p);
    }
    if (defined $opt_D) {
        die "Tabular is not handled yet in new C++ binaries!\n";
    }

    return $cmd_prefix . $retval . $cmd_suffix;
}

sub handle_rpsblast
{
    my $print_only = shift;
    my ($opt_F, $opt_I, $opt_J, $opt_L, $opt_N, $opt_O, $opt_P, $opt_T, 
        $opt_U, $opt_V, $opt_X, $opt_Y, $opt_Z, $opt_a, $opt_b, $opt_d, 
        $opt_e, $opt_i, $opt_l, $opt_m, $opt_o, $opt_p, $opt_v, $opt_y, 
        $opt_z);

    GetOptions("<>"             => sub { $application = shift; },
               "print_only!"    => $print_only,
               "F=s"            => \$opt_F,
               "I:s"            => \$opt_I,
               "J:s"            => \$opt_J,
               "L=s"            => \$opt_L,
               "N=f"            => \$opt_N,
               "O=s"            => \$opt_O,
               "P=i"            => \$opt_P,
               "T:s"            => \$opt_T,
               "U:s"            => \$opt_U,
               "V=s"            => \$opt_V,
               "X=i"            => \$opt_X,
               "Y=f"            => \$opt_Y,
               "Z=i"            => \$opt_Z,
               "a=i"            => \$opt_a,
               "b=i"            => \$opt_b,
               "d=s"            => \$opt_d,
               "e=f"            => \$opt_e,
               "i=s"            => \$opt_i,
               "l=s"            => \$opt_l,
               "m=i"            => \$opt_m,
               "o=s"            => \$opt_o,
               "p:s"            => \$opt_p,
               "v=i"            => \$opt_v,
               "y=f"            => \$opt_y,
               "z=f"            => \$opt_z
               );
    my $retval;

    if (defined $opt_p and $opt_p =~ /f/i) {
        $retval = "./rpstblastn ";
    } else {
        $retval = "./rpsblast ";
    }

    $retval .= "-query $opt_i "             if (defined $opt_i);
    $retval .= "-db \"$opt_d\" "            if (defined $opt_d);
    $retval .= "-evalue $opt_e "            if (defined $opt_e);
    $retval .= "-out $opt_o "               if (defined $opt_o);
    $retval .= "-xdrop_ungap $opt_y "       if (defined $opt_y);
    $retval .= "-xdrop_gap $opt_X "         if (defined $opt_X);
    $retval .= "-min_raw_gapped_score $opt_N " if (defined $opt_N);
    $retval .= "-num_threads $opt_a "       if (defined $opt_a);
    $retval .= "-num_descriptions $opt_v "  if (defined $opt_v);
    $retval .= "-num_alignments $opt_b "    if (defined $opt_b);
    $retval .= "-dbsize $opt_z "            if (defined $opt_z);
    $retval .= "-searchsp $opt_Y "          if (defined $opt_Y);
    $retval .= "-xdrop_gap_final $opt_Z "   if (defined $opt_Z);
    if (defined $opt_m) {
        if ($opt_m == 5 or $opt_m == 6) {
            print STDERR "Warning: -m5 or -m6 formatting options ";
            print STDERR "are not supported!\n";
        }
        $opt_m -= 2 if ($opt_m >= 7);
        $retval .= "-outfmt $opt_m "            
    }
    if (defined $opt_O) {
        unless ($retval =~ s/-out \S+ /-out $opt_O /) {
            $retval .= "-out $opt_O ";
        }
        unless ($retval =~ s/-outfmt \d+/-outfmt 8/) {
            $retval .= "-outfmt 8 ";
        } else {
            print STDERR "Warning: overriding output format\n";
        }
    }
    if (defined $opt_T and (length($opt_T) == 0 or $opt_T =~ /t/i)) {
        $retval .= "-html "                     
    }
    if (defined $opt_P and $opt_P eq "1") {
        $retval .= "-window_size 0 ";
    }
    if (defined $opt_F) {
        $retval .= &convert_filter_string($opt_F, "blastp");
    }
    if (defined $opt_I and (length($opt_I) == 0 or $opt_I =~ /t/i)) {
        $retval .= "-show_gis ";
    }
    if (defined $opt_J and (length($opt_J) == 0 or $opt_J =~ /t/i)) {
        $retval .= "-parse_query_defline ";
    }
    if (defined $opt_U and (length($opt_U) == 0 or $opt_U =~ /t/i)) {
        $retval .= "-lcase_masking ";
    }
    $retval .= &convert_sequence_locations($opt_L, "query") if ($opt_L);

    return $retval;
}

__END__

=head1 NAME

B<legacy_blast.pl> - Convert BLAST command line invocations from NCBI C 
toolkit's implementation to NCBI C++ toolkit's implementation.

=head1 SYNOPSIS

legacy_blast.pl <C toolkit command line program and arguments> [--print_only] 

=head1 OPTIONS

=over 2

=item B<--print_only>

Print the equivalent command line option instead of running the command
(default: false). This option MUST be the last argument to the script.

=head1 DESCRIPTION

This script converts and runs the equivalent NCBI C toolkit command line BLAST 
program and arguments provided to it (whenever possible) to NCBI C++ tookit 
BLAST programs.

=head1 EXIT CODES

This script returns 0 on success and a non-zero value on errors.

=head1 BUGS

Please report them to <blast-help@ncbi.nlm.nih.gov>

=head1 COPYRIGHT

See PUBLIC DOMAIN NOTICE included at the top of this script.

=cut
