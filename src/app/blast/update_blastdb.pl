#!/usr/bin/env perl
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
#   Script to download the pre-formatted BLAST databases.
#
# ===========================================================================

use strict;
use warnings;
use Net::FTP;
use Getopt::Long;
use Pod::Usage;
use File::stat;
use Digest::MD5;
use Archive::Tar;
use File::Temp;
use JSON::PP;

use constant NCBI_FTP => "ftp.ncbi.nlm.nih.gov";
use constant BLAST_DB_DIR => "/blast/db";
use constant USER => "anonymous";
use constant PASSWORD => "anonymous";
use constant DEBUG => 0;
use constant MAX_DOWNLOAD_ATTEMPTS => 3;
use constant EXIT_FAILURE => 1;

use constant AWS_URL => "http://s3.amazonaws.com";
use constant AMI_URL => "http://169.254.169.254/latest/meta-data/local-hostname";
use constant AWS_BUCKET => "ncbi-blast-databases";

use constant GCS_URL => "https://storage.googleapis.com";
use constant GCP_URL => "http://metadata.google.internal/computeMetadata/v1/instance/id";
use constant GCP_BUCKET => "blast-db";

use constant BLASTDB_MANIFEST => "blastdb-manifest.json";
use constant BLASTDB_MANIFEST_VERSION => "1.0";

# Process command line options
my $opt_verbose = 1;
my $opt_quiet = 0;
my $opt_force_download = 0;     
my $opt_help = 0;
my $opt_passive = 1;
my $opt_blastdb_ver = undef;
my $opt_timeout = 120;
my $opt_showall = undef;
my $opt_show_version = 0;
my $opt_decompress = 0;
my $opt_source;
my $opt_legacy_exit_code = 0;
my $opt_nt = &get_num_cores();
my $result = GetOptions("verbose+"          =>  \$opt_verbose,
                        "quiet"             =>  \$opt_quiet,
                        "force"             =>  \$opt_force_download,
                        "passive:s"         =>  \$opt_passive,
                        "timeout=i"         =>  \$opt_timeout,
                        "showall:s"         =>  \$opt_showall,
                        "version"           =>  \$opt_show_version,
                        "blastdb_version:i" =>  \$opt_blastdb_ver,
                        "decompress"        =>  \$opt_decompress,
                        "source=s"          =>  \$opt_source,
                        "num_threads=i"     =>  \$opt_nt,
                        "legacy_exit_code"  =>  \$opt_legacy_exit_code,
                        "help"              =>  \$opt_help);
$opt_verbose = 0 if $opt_quiet;
die "Failed to parse command line options\n" unless $result;
pod2usage({-exitval => 0, -verbose => 2}) if $opt_help;
if (length($opt_passive) and ($opt_passive !~ /1|no/i)) {
    pod2usage({-exitval => 1, -verbose => 0,
            -msg => "Invalid value for passive option: '$opt_passive'"});
}
pod2usage({-exitval => 0, -verbose => 2}) unless (scalar @ARGV or 
                                                  defined($opt_showall) or
                                                  $opt_show_version);
if (defined $opt_blastdb_ver) {
    pod2usage({-exitval => 1, -verbose => 0, 
               -msg => "Invalid BLAST database version: $opt_blastdb_ver. Supported values: 4, 5"}) 
        unless ($opt_blastdb_ver == 4 or $opt_blastdb_ver == 5);
}
pod2usage({-exitval => 1, -verbose => 0, -msg => "Invalid number of threads"}) 
    if ($opt_nt <= 0);
if (length($opt_passive) and $opt_passive =~ /n|no/i) {
    $opt_passive = 0;
} else {
    $opt_passive = 1;
}
my $exit_code = 0;
$|++;

if ($opt_show_version) {
    my $revision = '$Revision$';
    $revision =~ s/\$Revision: | \$//g;
    print "$0 version $revision\n";
    exit($exit_code);
}
my $curl = &get_curl_path();

my $location = "NCBI";
# If provided, the source takes precedence over any attempts to determine the closest location
if (defined($opt_source)) {
    if ($opt_source =~ /^ncbi/i) {
        $location = "NCBI";
    } elsif ($opt_source =~ /^gc/i) {
        $location = "GCP";
    } elsif ($opt_source =~ /^aws/i) {
        $location = "AWS";
    }
} else {
    # Try to auto-detect whether we're on the cloud
    if (defined($curl)) {
        my $tmpfile = File::Temp->new();
        my $gcp_cmd = "$curl --connect-timeout 3 --retry 3 --retry-max-time 30 -sfo $tmpfile -H 'Metadata-Flavor: Google' " . GCP_URL;
        my $aws_cmd = "$curl --connect-timeout 3 --retry 3 --retry-max-time 30 -sfo /dev/null " . AMI_URL;
        print "$gcp_cmd\n" if DEBUG;
        if (system($gcp_cmd) == 0) { 
            # status not always reliable.  Check that curl output is all digits.
            my $tmpfile_content = do { local $/; <$tmpfile>};
            print "curl output $tmpfile_content\n" if DEBUG;
            $location = "GCP" if ($tmpfile_content =~ m/^(\d+)$/);
        } elsif (DEBUG) {
            # Consult https://ec.haxx.se/usingcurl/usingcurl-returns
            print "curl to GCP metadata server returned ", $?>>8, "\n";
        }

        print "$aws_cmd\n" if DEBUG;
        if (system($aws_cmd) == 0) {
            $location = "AWS";
        } elsif (DEBUG) {
            # Consult https://ec.haxx.se/usingcurl/usingcurl-returns
            print "curl to AWS metadata server returned ", $?>>8, "\n";
        }
        print "Location is $location\n" if DEBUG;
    }
}
if ($location =~ /aws|gcp/i and not defined $curl) {
    print "Error: $0 depends on curl to fetch data from cloud storage, please install this utility to access these data sources.\n";
    exit(EXIT_FAILURE);
}

my $ftp;

if ($location ne "NCBI") {
    die "Only BLASTDB version 5 is supported at GCP and AWS\n" if (defined $opt_blastdb_ver and $opt_blastdb_ver != 5);
    my $latest_dir = &get_latest_dir($location);
    my ($json, $url) = &get_blastdb_metadata($location, $latest_dir);
    unless (length($json)) {
        print STDERR "ERROR: Missing manifest file $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    print "Connected to $location\n" if $opt_verbose;
    my $metadata = decode_json($json);
    unless (exists($$metadata{version}) and ($$metadata{version} eq BLASTDB_MANIFEST_VERSION)) {
        print STDERR "ERROR: Invalid version in manifest file $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    if (defined($opt_showall)) {
        my $print_header = 1;
        foreach my $db (sort keys %$metadata) {
            next if ($db =~ /^version$/);
            if ($opt_showall =~ /tsv/i) {
                printf("%s\t%s\t%9.4f\t%s\n", $db, $$metadata{$db}{description}, 
                    $$metadata{$db}{size}, $$metadata{$db}{last_updated});
            } elsif ($opt_showall =~ /pretty/i) {
                if ($print_header) {
                    printf("%-60s %-120s %-11s %15s\n", "BLASTDB", 
                        "DESCRIPTION", "SIZE (GB)", "LAST_UPDATED");
                    $print_header = 0;
                }
                printf("%-60s %-120s %9.4f %15s\n", $db, $$metadata{$db}{description}, 
                    $$metadata{$db}{size}, $$metadata{$db}{last_updated});
            } else {
                print "$db\n";
            }
        }
    } else {
        my @files2download;
        for my $requested_db (@ARGV) {
            if (exists $$metadata{$requested_db}) {
                push @files2download, @{$$metadata{$requested_db}{files}};
            } else {
                print STDERR "Warning: $requested_db does not exist in $location ($latest_dir)\n";
            }
        }
        if (@files2download) {
            my $gsutil = &get_gsutil_path();
            my $awscli = undef; # &get_awscli_path(); # aws s3 required credentials, fall back to curl
            my $cmd;
            my $fh = File::Temp->new();
            if ($location eq "GCP" and defined($gsutil)) {
                $cmd = "$gsutil ";
                if ($opt_nt > 1) {
                    $cmd .= "-m -q ";
                    $cmd .= "-o 'GSUtil:parallel_thread_count=1' -o 'GSUtil:parallel_process_count=$opt_nt' ";
                    $cmd .= "cp ";
                } else {
                    $cmd .= "-q cp ";
                }
                $cmd .= join(" ", @files2download) . " .";
            } elsif ($location eq "AWS" and defined ($awscli)) {
                my $aws_cmd = "$awscli s3 cp ";
                $aws_cmd .= "--only-show-errors " unless $opt_verbose >= 3;
                print $fh join("\n", @files2download);
                $cmd = "/usr/bin/xargs -P $opt_nt -n 1 -I{}";
                $cmd .= " -t" if $opt_verbose > 3;
                $cmd .= " $aws_cmd {} .";
                $cmd .= " <$fh " ;
            } else { # fall back to  curl
                my $url = $location eq "AWS" ? AWS_URL : GCS_URL;
                s,gs://,$url/, foreach (@files2download);
                s,s3://,$url/, foreach (@files2download);
                if ($opt_nt > 1 and -f "/usr/bin/xargs") {
                    print $fh join("\n", @files2download);
                    $cmd = "/usr/bin/xargs -P $opt_nt -n 1";
                    $cmd .= " -t" if $opt_verbose > 3;
                    $cmd .= " $curl -sSOR";
                    $cmd .= " <$fh " ;
                } else {
                    $cmd = "$curl -sSR";
                    $cmd .= " -O $_" foreach (@files2download);
                }
            }
            print "$cmd\n" if $opt_verbose > 3;
            system($cmd);
        }
    }

} else {
    # Connect and download files
    $ftp = &connect_to_ftp();
    if (defined $opt_showall) {
        print "$_\n" foreach (sort(&get_available_databases($ftp->ls())));
    } else {
        my @files = sort(&get_files_to_download());
        my @files2decompress;
        $exit_code = &download(\@files, \@files2decompress);
        if ($exit_code == 1) {
            foreach (@files2decompress) {
                $exit_code = &decompress($_);
                last if ($exit_code != 1);
            }
        }
        unless ($opt_legacy_exit_code) {
            $exit_code = ($exit_code == 1 ? 0 : $exit_code);
        }
    }
    $ftp->quit();
}

exit($exit_code);

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
    my $ftp_path = BLAST_DB_DIR;
    $ftp_path .= "/v$opt_blastdb_ver" if (defined $opt_blastdb_ver);
    $ftp->cwd($ftp_path);
    $ftp->binary();
    if ($opt_verbose) {
        if (defined $opt_blastdb_ver) {
            print "Connected to $location; downloading BLASTDBv$opt_blastdb_ver\n";
        } else {
            print "Connected to $location\n";
        }
    }
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
    my %seen = ();
    return grep { ! $seen{$_} ++ } @retval;
}

# Obtains the list of files to download
sub get_files_to_download
{
    my @blast_db_files = $ftp->ls();
    my @retval = ();

    if ($opt_verbose > 2) {
        print "Found the following files on ftp site:\n";
        print "$_\n" for (@blast_db_files);
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

# Download the requested files only if their checksum files are missing or if
# these (or the archives) are newer in the FTP site. Returns 0 if no files were
# downloaded, 1 if at least one file was downloaded (so that this can be the
# application's exit code)
sub download($$)
{
    my @requested_dbs = @ARGV;
    my @files2download = @{$_[0]};
    my $files2decompress = $_[1];
    my $retval = 0;

    for my $file (@files2download) {

        my $attempts = 0;   # Download attempts for this file
        if ($opt_verbose and &is_multivolume_db($file) and $file =~ /\.00\./) {
            my $db_name = &extract_db_name($file);
            my $nvol = &get_num_volumes($db_name, @files2download);
            print "Downloading $db_name (" . $nvol . " volumes) ...\n" unless ($opt_quiet);
        }

        # We preserve the checksum files as evidence of the downloaded archive
        my $checksum_file = "$file.md5";
        my $new_download = (-e $checksum_file ? 0 : 1);
        my $update_available = ($new_download or 
                    ((stat($checksum_file))->mtime < $ftp->mdtm($checksum_file)));
        if (-e $file and (stat($file)->mtime < $ftp->mdtm($file))) {
            $update_available = 1;
        }

download_file:
        if ($opt_force_download or $new_download or $update_available) {
            print "Downloading $file..." if $opt_verbose;
            $ftp->get($file);
            unless ($ftp->get($checksum_file)) {
                print STDERR "Failed to download $checksum_file!\n";
                return EXIT_FAILURE;
            }
            my $rmt_digest = &read_md5_file($checksum_file);
            my $lcl_digest = &compute_md5_checksum($file);
            print "\nRMT $file Digest $rmt_digest" if (DEBUG);
            print "\nLCL $file Digest $lcl_digest\n" if (DEBUG);
            if ($lcl_digest ne $rmt_digest) {
                unlink $file, $checksum_file;
                if (++$attempts >= MAX_DOWNLOAD_ATTEMPTS) {
                    print STDERR "too many failures, aborting download!\n";
                    return EXIT_FAILURE;
                } else {
                    print "corrupt download, trying again.\n";
                    goto download_file;
                }
            }
            push @$files2decompress, $file if ($opt_decompress);
            print " [OK]\n" if $opt_verbose;
            $retval = 1 if ($retval == 0);
        } else {
            if ($opt_decompress and -f $file) {
                push @$files2decompress, $file;
                $retval = 1;
            } else {
                my $msg = ($opt_decompress 
                           ? "The contents of $file are up to date in your system." 
                           : "$file is up to date.");
                print "$msg\n" if $opt_verbose;
            }
        }
    }
    return $retval;
}

# Try to decompress using /bin/tar as Archive::Tar is known to be slower (as
# it's pure perl)
sub _decompress_impl($)
{
    my $file = shift;
    if ($^O eq "cygwin") {
        local $ENV{PATH} = "/bin:/usr/bin";
        my $cmd = "tar -zxf $file 2>/dev/null";
        return 1 unless (system($cmd));
    }
    unless ($^O =~ /win/i) {
        local $ENV{PATH} = "/bin:/usr/bin";
        my $cmd = "gzip -cd $file 2>/dev/null | tar xf - 2>/dev/null";
        return 1 unless (system($cmd));
    }
    return Archive::Tar->extract_archive($file, 1);
}

# Decompresses the file passed as its argument
# Returns 1 on success, and 2 on failure, printing an error to STDERR
sub decompress($)
{
    my $file = shift;
    print "Decompressing $file ..." unless ($opt_quiet);
    my $succeeded = &_decompress_impl($file);
    unless ($succeeded) {
        my $msg = "Failed to decompress $file ($Archive::Tar::error), ";
        $msg .= "please do so manually.";
        print STDERR "$msg\n";
        return EXIT_FAILURE;
    }
    unlink $file;   # Clean up archive, but preserve the checksum file
    print " [OK]\n" unless ($opt_quiet);
    return 1;
}

sub compute_md5_checksum($)
{
    my $file = shift;
    my $digest = "N/A";
    if (open(DOWNLOADED_FILE, $file)) {
        binmode(DOWNLOADED_FILE);
        $digest = Digest::MD5->new->addfile(*DOWNLOADED_FILE)->hexdigest;
        close(DOWNLOADED_FILE);
    }
    return $digest;
}

sub read_md5_file($)
{
    my $md5file = shift;
    open(IN, $md5file);
    $_ = <IN>;
    close(IN);
    my @retval = split;
    return $retval[0];
}

# Determine if a given pre-formatted BLAST database file is part of a
# multi-volume database
sub is_multivolume_db
{
    my $file = shift;
    return 1 if ($file =~ /\.\d{2,3}\.tar\.gz$/);
    return 0;
}

# Extracts the database name from the pre-formatted BLAST database archive file
# name
sub extract_db_name
{
    my $file = shift;
    my $retval = "";
    if (&is_multivolume_db($file)) {
        $retval = $1 if ($file =~ m/(.*)\.\d{2,3}\.tar\.gz$/);
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
            if (/.*\.(\d{2,3})\.tar\.gz$/) {
                $retval = int($1) if (int($1) > $retval);
            }
        }
    }
    return $retval + 1;
}

# Retrieves the name of the 'subdirectory' where the latest BLASTDBs residue in GCP
sub get_latest_dir
{
    my $source = shift;
    my $url = GCS_URL . "/" . GCP_BUCKET . "/latest-dir";
    $url = AWS_URL . "/" . AWS_BUCKET . "/latest-dir" if ($source eq "AWS");
    my $cmd = "$curl -s $url";
    print "$cmd\n" if DEBUG;
    chomp(my $retval = `$cmd`);
    unless (length($retval)) {
        print STDERR "ERROR: Missing file $url, please try again or report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    print "$source latest-dir: '$retval'\n" if DEBUG;
    return $retval;
}

# Fetches the JSON text containing the BLASTDB metadata in GCS
sub get_blastdb_metadata
{
    my $source = shift;
    my $latest_dir = shift;
    my $url = GCS_URL . "/" . GCP_BUCKET . "/$latest_dir/" . BLASTDB_MANIFEST;
    $url = AWS_URL . "/" . AWS_BUCKET . "/$latest_dir/" . BLASTDB_MANIFEST if ($source eq "AWS");
    my $cmd = "curl -sf $url";
    print "$cmd\n" if DEBUG;
    chomp(my $retval = `$cmd`);
    return ($retval, $url);
}

# Returns the path to the gsutil utility or undef if it is not found
sub get_gsutil_path
{
    foreach (qw(/google/google-cloud-sdk/bin /usr/local/bin /usr/bin /snap/bin)) {
        my $path = "$_/gsutil";
        return $path if (-f $path);
    }
    return undef;
}

# Returns the path to the aws CLI utility or undef if it is not found
sub get_awscli_path
{
    foreach (qw(/usr/local/bin /usr/bin)) {
        my $path = "$_/aws";
        return $path if (-f $path);
    }
    return undef;
}

# Returns the number of cores, or 1 if unknown
sub get_num_cores
{
    my $retval = 1;
    if ($^O =~ /linux/i) {
        open my $fh, "/proc/cpuinfo" or return $retval;
        $retval = scalar(map /^processor/, <$fh>);
        close($fh);
    } elsif ($^O =~ /darwin/i) {
        chomp($retval = `/usr/sbin/sysctl -n hw.ncpu`);
    }
    return $retval;
}

# Returns the path to the curl utility, or undef if it is not found
sub get_curl_path
{
    foreach (qw(/usr/local/bin /usr/bin)) {
        my $path = "$_/curl";
        return $path if (-f $path);
    }
    if ($^O =~ /mswin/i) {
        my $retval = `where curl`;
        if (defined $retval) {
            chomp($retval);
            return $retval if (-f $retval);
        }
    }
    return undef;
}

__END__

=head1 NAME

B<update_blastdb.pl> - Download pre-formatted BLAST databases

=head1 SYNOPSIS

update_blastdb.pl [options] blastdb ...

=head1 OPTIONS

=over 2

=item B<--source>

Location to download BLAST databases from (default: auto-detect closest location).
Supported values: ncbi, aws, or gcp.

=item B<--decompress>

Downloads, decompresses the archives in the current working directory, and
deletes the downloaded archive to save disk space, while preserving the
archive checksum files (default: false).

=item B<--showall>

Show all available pre-formatted BLAST databases (default: false). The output
of this option lists the database names which should be used when
requesting downloads or updates using this script.

It accepts the optional arguments: 'tsv' and 'pretty' to produce tab-separated values
and a human-readable format respectively. These parameters elicit the display of
additional metadata if this is available to the program.
This metadata is displayed in columnar format; the columns represent:

name, description, size in gigabytes, date of last update (YYYY-MM-DD format).

=item B<--blastdb_version>

Specify which BLAST database version to download (default: 5).
Supported values: 4, 5

=item B<--passive>

Use passive FTP, useful when behind a firewall or working in the cloud (default: true).
To disable passive FTP, configure this option as follows: --passive no

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

=item B<--version>

Prints this script's version. Overrides all other options.

=item B<--num_threads>

Sets the number of threads to utilize to perform downloads in parallel when data comes from the cloud.
Defaults to use all available CPUs on the machine (Linux and macos only).

=item B<--legacy_exit_code>

Enables exit codes from prior to version 581818, BLAST+ 2.10.0 release, for
downloads from NCBI only. This option is meant to be used by legacy applications that rely
on this exit codes:
0 for successful operations that result in no downloads, 1 for successful
downloads, and 2 for errors.

=back

=head1 DESCRIPTION

This script will download the pre-formatted BLAST databases requested in the
command line from the NCBI ftp site.

=head1 EXIT CODES

This script returns 0 on successful operations and non-zero on errors.

=head1 DEPENDENCIES

This script depends on curl for retrieval from cloud service providers.

=head1 BUGS

Please report them to <blast-help@ncbi.nlm.nih.gov>

=head1 COPYRIGHT

See PUBLIC DOMAIN NOTICE included at the top of this script.

=cut
