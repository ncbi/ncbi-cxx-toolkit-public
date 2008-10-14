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
#   Script to download the pre-formatted BLAST databases from the NCBI ftp
#   server.
#
# ===========================================================================

use strict;
use warnings;
use Net::FTP;
use Getopt::Long;
use Pod::Usage;
use File::stat;

use constant VERSION => 1.2;
use constant NCBI_FTP => "ftp.ncbi.nlm.nih.gov";
use constant BLAST_DB_DIR => "/blast/db";
use constant USER => "anonymous";
use constant PASSWORD => "anonymous";
use constant DEBUG => 0;

# Process command line options
my $opt_verbose = 1;
my $opt_quiet = 0;
my $opt_force_download = 0;     
my $opt_help = 0;
my $opt_passive = 0;
my $opt_timeout = 120;
my $opt_showall = 0;
my $result = GetOptions("verbose+"  =>  \$opt_verbose,
                        "quiet"     =>  \$opt_quiet,
                        "force"     =>  \$opt_force_download,
                        "passive"   =>  \$opt_passive,
                        "timeout=i" =>  \$opt_timeout,
                        "showall"   =>  \$opt_showall,
                        "help"      =>  \$opt_help);
$opt_verbose = 0 if $opt_quiet;
die "Failed to parse command line options\n" unless $result;
pod2usage({-exitval => 0, -verbose => 2}) if $opt_help;
pod2usage({-exitval => 1, -verbose => 2}) unless (scalar @ARGV or $opt_showall);


# Connect and download files
my $ftp = &connect_to_ftp();
if ($opt_showall) {
    print "$_\n" foreach (sort(&get_available_databases()));
} else {
    my @files = sort(&get_files_to_download());
    &download(@files);
}
$ftp->quit();

# Connects to NCBI ftp server
sub connect_to_ftp
{
    my %ftp_opts;
    $ftp_opts{'Passive'} = 1 if $opt_passive;
    $ftp_opts{'Timeout'} = $opt_timeout if ($opt_timeout >= 0);
    $ftp_opts{'Debug'}   = 1 if ($opt_verbose > 1);
    my $ftp = Net::FTP->new(NCBI_FTP, %ftp_opts)
        or die "Failed to connect to " . NCBI_FTP . ": $!\n";
    $ftp->login(USER, PASSWORD) 
        or die "Failed to login to " . NCBI_FTP . ": $!\n";
    $ftp->cwd(BLAST_DB_DIR);
    $ftp->binary();
    print STDERR "Connected to NCBI\n" if $opt_verbose;
    return $ftp;
}

# Gets the list of available databases on NCBI FTP site
sub get_available_databases
{
    my @blast_db_files = $ftp->ls();
    my @retval = ();

    foreach (@blast_db_files) {
        next unless (/\.tar\.gz$/);
        push @retval, &extract_db_name($_);
    }

    # Sort and eliminate adjacent duplicates
    @retval = sort @retval;
    my $prev = "not equal to $retval[0]";
    return grep($_ ne $prev && ($prev = $_, 1), @retval);
}

# Obtains the list of files to download
sub get_files_to_download
{
    my @blast_db_files = $ftp->ls();
    my @retval = ();

    if (DEBUG) {
        print STDERR "DEBUG: Found the following files on ftp site:\n";
        print STDERR "DEBUG: $_\n" for (@blast_db_files);
    }

    for my $requested_db (@ARGV) {
        for my $file (@blast_db_files) {
            next unless ($file =~ /\.tar\.gz$/);    
            if ($file =~ /^$requested_db\..*/) {
                push @retval, $file;
            }
        }
    }

    if ($opt_verbose) {
        for my $requested_db (@ARGV) {
            unless (grep(/$requested_db/, @retval)) {
                print STDERR "$requested_db not found, skipping.\n" 
            }
        }
    }

    return @retval;
}

# Download the requestes files only if they are missing or if they are newer in
# the FTP site.
sub download($)
{
    my @requested_dbs = @ARGV;

    for my $file (@_) {

        if ($opt_verbose and &is_multivolume_db($file)) {
            my $db_name = &extract_db_name($file);
            my $nvol = &get_num_volumes($db_name, @_);
            print STDERR "Downloading $db_name (" . $nvol . " volumes) ...\n";
        }

        if ($opt_force_download or
            not -f $file or 
            ((stat($file))->mtime < $ftp->mdtm($file))) {
            print STDERR "Downloading $file... " if $opt_verbose;
            $ftp->get($file);
            print STDERR "done.\n" if $opt_verbose;
        } else {
            print STDERR "$file is up to date.\n" if $opt_verbose;
        }
    }
}

# Determine if a given pre-formatted BLAST database file is part of a
# multi-volume database
sub is_multivolume_db
{
    my $file = shift;
    return 1 if ($file =~ /\.\d{2}\.tar\.gz$/);
    return 0;
}

# Extracts the database name from the pre-formatted BLAST database archive file
# name
sub extract_db_name
{
    my $file = shift;
    my $retval = "";
    if (&is_multivolume_db($file)) {
        $retval = $1 if ($file =~ m/(.*)\.\d{2}\.tar\.gz$/);
    } else {
        $retval = $1 if ($file =~ m/(.*)\.tar\.gz$/);
    }
    return $retval;
}

# Returns the number of volumes for a BLAST database given the file name of a
# pre-formatted BLAST database and the list of all databases to download
sub get_num_volumes
{
    my $db = shift;
    my $retval = 0;
    foreach (@_) {
        if (/$db/) {
            if (/.*\.(\d{2})\.tar\.gz$/) {
                $retval = int($1) if (int($1) > $retval);
            }
        }
    }
    return $retval + 1;
}

__END__

=head1 NAME

B<update_blastdb.pl> - Download pre-formatted BLAST databases from NCBI

=head1 SYNOPSIS

update_blastdb.pl [options] blastdb ...

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
