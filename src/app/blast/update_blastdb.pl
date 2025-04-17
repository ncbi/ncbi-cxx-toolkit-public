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
use constant LEGACY_EXIT_FAILURE => 2;

use constant AWS_URL => "http://s3.amazonaws.com";
use constant AMI_URL => "http://169.254.169.254/latest/meta-data/local-hostname";
use constant AWS_BUCKET => "ncbi-blast-databases";

use constant GCS_URL => "https://storage.googleapis.com";
use constant GCP_URL => "http://metadata.google.internal/computeMetadata/v1/instance/id";
use constant GCP_BUCKET => "blast-db";

use constant BLASTDB_METADATA => "blastdb-metadata-1-1.json";
use constant BLASTDB_METADATA_VERSION => "1.1";

# Process command line options
my $opt_verbose = 1;
my $opt_quiet = 0;
my $opt_force_download = 0;     
my $opt_help = 0;
my $opt_passive = 1;
my $opt_timeout = 120;
my $opt_showall = undef;
my $opt_show_version = 0;
my $opt_decompress = 0;
my $opt_source;
my $opt_legacy_exit_code = 0;
my $opt_nt = &compute_default_num_threads();
my $opt_gcp_prj = undef;
my $opt_use_ftp_protocol = 0;
my $result = GetOptions("verbose+"          =>  \$opt_verbose,
                        "quiet"             =>  \$opt_quiet,
                        "force"             =>  \$opt_force_download,
                        "passive:s"         =>  \$opt_passive,
                        "timeout=i"         =>  \$opt_timeout,
                        "showall:s"         =>  \$opt_showall,
                        "version"           =>  \$opt_show_version,
                        "decompress"        =>  \$opt_decompress,
                        "source=s"          =>  \$opt_source,
                        "gcp-project=s"     =>  \$opt_gcp_prj,
                        "num_threads=i"     =>  \$opt_nt,
                        "legacy_exit_code"  =>  \$opt_legacy_exit_code,
                        "force_ftp"         =>  \$opt_use_ftp_protocol,
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
pod2usage({-exitval => 1, -verbose => 0, -msg => "Invalid number of threads"}) 
    if ($opt_nt < 0);
my $max_num_cores = &get_num_cores();
$opt_nt = $max_num_cores if ($opt_nt == 0);
if ($opt_nt > $max_num_cores) {
    print STDERR "WARNING: num_threads setting of $opt_nt is higher than the available number of cores ($max_num_cores); resetting it to $max_num_cores\n";
    $opt_nt = $max_num_cores;
}

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
my $gsutil = &get_gsutil_path();
my $gcloud = &get_gcloud_path();

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
            # When running in GCP, set the data source location as NCBI
            $location = "NCBI" if ($tmpfile_content =~ m/^(\d+)$/);
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
if ($location =~ /aws/i and not defined $curl) {
    print "Error: $0 depends on curl to fetch data from cloud storage, please install this utility to access this data source.\n";
    exit(EXIT_FAILURE);
}
if ($location =~ /gcp/i and defined($opt_gcp_prj) and (not defined $gsutil or not defined $gcloud)) {
    print "Error: when providing a GCP project, $0 depends on gsutil and gcloud to fetch data from cloud storage, please install these utilities to access this data source.\n";
    exit(EXIT_FAILURE);
}
my $gcp_prj = $opt_gcp_prj;
#my $gcp_prj = ($location =~ /gcp/i) ? &get_gcp_project() : undef;
#if ($location =~ /gcp/i and not defined $gcp_prj) {
#    print "Error: $0 depends on gcloud being configured to fetch data from cloud storage, please configure it per the instructions in https://cloud.google.com/sdk/docs/initializing .\n";
#    exit(EXIT_FAILURE);
#}

my $ftp;

sub validate_metadata_file
{
    my $json = shift;
    my $url = shift;
    my $metadata = decode_json($json);
    if (ref($metadata) eq 'HASH') {
        # This code branch detects the manifest version below
        # We switched to manifest version 1.1 on 2022-07-28, for the BLAST+ 2.14.0 release
        use constant BLASTDB_MANIFEST_VERSION => "1.0";
        unless (exists($$metadata{version}) and ($$metadata{version} eq BLASTDB_MANIFEST_VERSION)) {
            print STDERR "ERROR: Invalid version in manifest file $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
            exit(EXIT_FAILURE);
        }
    } elsif (not ref($metadata) eq 'ARRAY') {
        print STDERR "ERROR: Invalid metadata format in $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
}

sub showall_from_metadata_file
{
    my $json = shift;
    my $url = shift;
    &validate_metadata_file($json, $url);
    my $metadata = decode_json($json);
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
}

# Display metadata from version 1.1 of BLASTDB metadata files
sub showall_from_metadata_file_1_1($$)
{
    my $json = shift;
    my $url = shift;
    &validate_metadata_file($json, $url);
    my $metadata = decode_json($json);
    my $print_header = 1;
    foreach my $db (sort @$metadata) {
        next if ($$db{version} ne BLASTDB_METADATA_VERSION);
        my $gb_total = sprintf("%.4f", $$db{'bytes-total'} * 1e-9);
        my $last_updated = $$db{'last-updated'};
        if ($opt_showall =~ /tsv/i) {
            printf("%s\t%s\t%9.4f\t%s\n", $$db{dbname}, $$db{description}, 
                $gb_total, $last_updated);
        } elsif ($opt_showall =~ /pretty/i) {
            if ($print_header) {
                printf("%-60s %-120s %-11s %15s\n", "BLASTDB", 
                    "DESCRIPTION", "SIZE (GB)", "LAST_UPDATED");
                $print_header = 0;
            }
            $last_updated = $$db{'last-updated'} =~ s/T.*//r;
            printf("%-60s %-120s %9.4f %15s\n", $$db{dbname}, $$db{description}, 
                $gb_total, $last_updated);
        } else {
            print "$$db{dbname}\n";
        }
    }
}

sub get_files_from_json_metadata_1_1($$)
{
    my $json = shift;
    my $url = shift;
    my @retval = ();
    &validate_metadata_file($json, $url);
    my $metadata = decode_json($json);
    foreach my $db (sort @$metadata) {
        next if ($$db{version} ne BLASTDB_METADATA_VERSION);
        push @retval, map { s,ftp://,https://,; $_; } @{$$db{files}};
    }
    return @retval;
}

if ($location ne "NCBI") {
    my $latest_dir = &get_latest_dir($location);
    my ($json, $url) = &get_blastdb_metadata($location, $latest_dir);
    unless (length($json)) {
        print STDERR "ERROR: Missing manifest file $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    print "Connected to $location\n" if $opt_verbose;
    print "Metadata source $url\n" if ($opt_verbose > 3);
    &validate_metadata_file($json, $url);
    my $metadata = decode_json($json);
    if (defined($opt_showall)) {
        &showall_from_metadata_file_1_1($json, $url);
    } else {
        &ensure_available_disk_space($json, $url);
        my @files2download;
        for my $requested_db (@ARGV) {
            my $found = 0;
            foreach my $dbm (sort @$metadata) {
                if ($$dbm{'dbname'} eq $requested_db) {
                    push @files2download, @{$$dbm{files}};
                    $found = 1;
                    last;
                }
            }
            if (not $found) {
                print STDERR "ERROR: $requested_db does not exist in $location ($latest_dir)\n";
                my $exit_code = ($opt_legacy_exit_code == 1 ? LEGACY_EXIT_FAILURE : EXIT_FAILURE);
                exit $exit_code;
            }
        }
        if (@files2download) {
            my $gsutil = &get_gsutil_path();
            my $awscli = &get_awscli_path();
            my $cmd;
            my $fh = File::Temp->new();
            if ($location eq "GCP" and defined($gcp_prj)) {
                $cmd = "$gsutil -u $gcp_prj ";
                if ($opt_nt > 1) {
                    $cmd .= "-m -q ";
                    $cmd .= "-o 'GSUtil:parallel_thread_count=1' -o 'GSUtil:parallel_process_count=$opt_nt' ";
                    $cmd .= "cp ";
                } else {
                    $cmd .= "-q cp ";
                }
                $cmd .= join(" ", @files2download) . " .";
            } elsif ($location eq "AWS" and defined ($awscli)) {
                # https://registry.opendata.aws/ncbi-blast-databases/#usageexamples
                my $aws_cmd = "$awscli s3 cp --no-sign-request ";
                $aws_cmd .= "--only-show-errors " unless $opt_verbose >= 3;
                print $fh join("\n", @files2download);
                $cmd = "/usr/bin/xargs -P $opt_nt -n 1 -I{}";
                $cmd .= " -t" if $opt_verbose > 3;
                $cmd .= " $aws_cmd {} .";
                $cmd .= " <$fh " ;
            } else { # fall back to curl
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
                    $cmd = "$curl -sSR --remote-name-all ";
                    $cmd .= join(" " , @files2download);
                }
            }
            print "$cmd\n" if $opt_verbose > 3;
            unless (system($cmd) == 0) {
                print STDERR "Failed to run '$cmd': $!!\n";
                return EXIT_FAILURE;
            }
        }
    }

} else {
    # Connect and download files
    $ftp = &connect_to_ftp();
    my ($json, $url) = &get_blastdb_metadata($location, '', $ftp);
    unless (length($json)) {
        print STDERR "ERROR: Missing manifest file $url, please report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    print "Metadata source $url\n" if ($opt_verbose > 3);

    if (defined $opt_showall) {
        &showall_from_metadata_file_1_1($json, $url)
    } else {
        &ensure_available_disk_space($json, $url);
        my @files = sort(&get_files_to_download($ftp, $json, $url));
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
    $ftp->quit() unless not defined($ftp);
}

exit($exit_code);

# Connects to NCBI ftp server
sub connect_to_ftp
{
    return undef if ($^O =~ /darwin/);  # Net::FTP appears unreliable on Mac

    # HTTPS access to NCBI FTP site works better than using the FTP protocol
    return undef unless ($opt_use_ftp_protocol);

    my %ftp_opts;
    $ftp_opts{'Passive'} = 1 if $opt_passive;
    $ftp_opts{'Timeout'} = $opt_timeout if ($opt_timeout >= 0);
    $ftp_opts{'Debug'}   = 1 if ($opt_verbose > 1);
    my $ftp = Net::FTP->new(NCBI_FTP, %ftp_opts)
        or die "Failed to connect to " . NCBI_FTP . ": $!\n";
    $ftp->login(USER, PASSWORD) 
        or die "Failed to login to " . NCBI_FTP . ": $!\n";
    my $ftp_path = BLAST_DB_DIR;
    $ftp->cwd($ftp_path);
    $ftp->binary();
    print "Connected to $location\n" if ($opt_verbose);
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

# Returns the last modified date for a file on the NCBI FTP or the number of
# seconds since epoch at the time of invocation of this function
sub get_last_modified_date_from_ncbi_ftp
{
    my $file = shift;
    my $retval = time();
    if (defined($ftp)) {
        $retval = $ftp->mdtm($file);
    } else {
        use Time::Local;
        my %month2int = (Jan=>0, Feb=>1, Mar=>2, Apr=>3, May=>4, Jun=>5,
            Jul=>6, Aug=>8, Sep=>8, Oct=>9, Nov=>10, Dec=>11);
        my $cmd = "$curl --user " . USER . ":" . PASSWORD . " -sI $file";
        if (open(my $fh, "-|", $cmd)) {
            while (<$fh>) {
                if (/Last-Modified:\s+\w+, (\d+) (\w+) (\d+) (\d+):(\d+):(\d+) GMT/) {
                    # Sample output: Wed, 07 Dec 2022 10:37:08 GMT
                    my $mday = int($1);
                    my $mon = $month2int{$2};
                    my $year = int($3) - 1900;
                    my $hour = int($4);
                    my $min = int($5);
                    my $sec = int($6);
                    $retval = Time::Local::timegm($sec, $min, $hour, $mday, $mon, $year);
                    print "$file $retval\n" if DEBUG;
                    last;
                }
            }
            close($fh);
        }
    }
    return $retval;
}

# This function exits the program if not enough disk space is available for the
# selected BLASTDBs
sub ensure_available_disk_space
{
    my $json = shift;
    my $url = shift;
    my $space_needed = 0;
    my $space_available = &get_available_disk_space;
    print "Available disk space in bytes: $space_available\n" if ($opt_verbose > 3);
    return unless $space_available;

    for my $requested_db (@ARGV) {
        my $x = &get_database_size_from_metadata_1_1($requested_db, $json, $url);
        $space_needed += $x if ($x > 0);
    }
    print "Needed disk space in bytes: $space_needed\n" if ($opt_verbose > 3);
    if ($space_needed > $space_available) {
        my $msg = "ERROR: Need $space_needed bytes and only ";
        $msg .= "$space_available bytes are available\n";
        print STDERR $msg;
        my $exit_code = ($opt_legacy_exit_code == 1 ? LEGACY_EXIT_FAILURE : EXIT_FAILURE);
        exit $exit_code;
    }
}

# Returns the available disk space in bytes of the current working directory
# Not supported in windows
sub get_available_disk_space
{
    my $retval = 0;
    return $retval if ($^O =~ /mswin/i);

    my $BLK_SIZE = 512;
    my $cmd = "df -P --block-size $BLK_SIZE .";
    $cmd = "df -P -b ." if ($^O =~ /darwin/i);
    foreach (`$cmd 2>/dev/null`) {
        chomp;
        next if (/^Filesystem/);
        my @F = split;
        $retval = $F[3] * $BLK_SIZE if (scalar(@F) == 6);
    }
    print STDERR "WARNING: unable to compute available disk space\n" unless $retval;
    return $retval;
}

sub get_database_size_from_metadata_1_1
{
    my $db = shift;
    my $json = shift;
    my $url = shift;
    my $retval = -1;
    &validate_metadata_file($json, $url);
    my $metadata = decode_json($json);
    foreach my $dbm (sort @$metadata) {
        if ($$dbm{'dbname'} eq $db) {
            $retval = $$dbm{'bytes-total'};
            last;
        }
    }
    print STDERR "Warning: No BLASTDB metadata for $db\n" if ($retval == -1);
    return $retval;
}

# Obtains the list of files to download
sub get_files_to_download($$$)
{
    my $ftp = shift;
    my $json = shift;
    my $url = shift;
    my @blast_db_files = (defined($ftp) 
        ? $ftp->ls() 
        : &get_files_from_json_metadata_1_1($json, $url));
    my @retval = ();

    if ($opt_verbose > 2) {
        print "Found the following files on ftp site:\n";
        print "$_\n" for (@blast_db_files);
    }

    for my $requested_db (@ARGV) {
        for my $file (@blast_db_files) {
            next unless ($file =~ /\.tar\.gz$/);    
            if (defined($ftp) and $file =~ /^$requested_db\..*/) {
                push @retval, $file;
            } elsif ($file =~ /\/$requested_db\..*/) {
                # for Mac, which no longer relies on Net::FTP
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
        my $rmt_checksum_file_mtime = &get_last_modified_date_from_ncbi_ftp($checksum_file);
        my $rmt_file_mtime = &get_last_modified_date_from_ncbi_ftp($file);
        my $lcl_checksum_file_mtime = (-e &rm_protocol($checksum_file)
            ? stat(&rm_protocol($checksum_file))->mtime : 0);
        print "RMT checksum file mtime $rmt_checksum_file_mtime\n" if DEBUG;
        print "LCL checksum file mtime $lcl_checksum_file_mtime\n" if DEBUG;
        my $update_available = ($lcl_checksum_file_mtime < $rmt_checksum_file_mtime);
        if (-e $file and (stat(&rm_protocol($file))->mtime < $rmt_file_mtime)) {
            $update_available = 1;
        }

download_file:
        if ($opt_force_download or $update_available) {
            print "Downloading $file..." if $opt_verbose;
            my $rv = 0;
            if (defined($ftp)) {
                # Download errors will be checked later when reading checksum files
                $ftp->get($file);
                $ftp->get($checksum_file);
            } else {
                my $cmd = "$curl --user " . USER . ":" . PASSWORD . " -sSR ";
                $cmd .= "--remote-name-all $file $file.md5";
                print "$cmd\n" if $opt_verbose > 3;
                $rv = system($cmd);
            }
            my ($rmt_digest, $lcl_digest) = (0)x2;
            if ($rv == 0) {
                $rmt_digest = &read_md5_file($checksum_file);
                $lcl_digest = &compute_md5_checksum($file);
            }
            print "\nRMT $file Digest $rmt_digest" if (DEBUG);
            print "\nLCL $file Digest $lcl_digest\n" if (DEBUG);
            if ($rv != 0 or ($lcl_digest ne $rmt_digest)) {
                unlink &rm_protocol($file), &rm_protocol($checksum_file);
                if (++$attempts >= MAX_DOWNLOAD_ATTEMPTS) {
                    print STDERR "too many failures, aborting download! Please try again later or use the --source aws option\n";
                    unlink $file, "$file.md5";
                    return -1;
                } else {
                    print "corrupt $file or $file.md5 download, trying again.\n";
                    sleep(exponential_delay($attempts));
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
    unless ($^O =~ /mswin/i) {
        local $ENV{PATH} = "/bin:/usr/bin";
        my $cmd = "gzip -cd $file 2>/dev/null | tar xf - 2>/dev/null";
        print "$cmd\n" if $opt_verbose > 3;
        return 1 unless (system($cmd));
    }
    return Archive::Tar->extract_archive($file, 1);
}

# Decompresses the file passed as its argument
# Returns 1 on success, and 2 on failure, printing an error to STDERR
sub decompress($)
{
    my $file = shift;
    $file = &rm_protocol($file);
    print "Decompressing $file ..." unless ($opt_quiet);
    my $succeeded = &_decompress_impl($file);
    unless ($succeeded) {
        my $msg = "Failed to decompress $file ($Archive::Tar::error), ";
        $msg .= "please do so manually.";
        print STDERR "$msg\n";
        return EXIT_FAILURE;
    }
    print "rm $file\n" if $opt_verbose > 3;
    unlink $file;   # Clean up archive, but preserve the checksum file
    print " [OK]\n" unless ($opt_quiet);
    return 1;
}

sub compute_md5_checksum($)
{
    my $file = shift;
    my $digest = "N/A";
    $file = &rm_protocol($file);
    if (open(DOWNLOADED_FILE, $file)) {
        binmode(DOWNLOADED_FILE);
        $digest = Digest::MD5->new->addfile(*DOWNLOADED_FILE)->hexdigest;
        close(DOWNLOADED_FILE);
    }
    return $digest;
}

# Removes the protocol prefix from the file name passed in
sub rm_protocol($)
{
    my $retval = shift;
    if ($retval =~ /^https:/) {
        my $prefix = "https://" . NCBI_FTP . BLAST_DB_DIR . "/";
        $retval =~ s/$prefix//;
    }
    return $retval;
}

sub read_md5_file($)
{
    my $md5file = shift;
    $md5file = &rm_protocol($md5file);
    return '' unless (-f $md5file);
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

# Retrieves the name of the 'subdirectory' where the latest BLASTDBs reside
sub get_latest_dir
{
    my $source = shift;
    my ($retval, $url, $cmd);
    if ($source eq "AWS") {
        $url = AWS_URL . "/" . AWS_BUCKET . "/latest-dir";
        $cmd = "$curl -s $url";
    } else {
        if (defined($gcp_prj)) {
            $url = 'gs://' . GCP_BUCKET . "/latest-dir";
            $cmd = "$gsutil -u $gcp_prj cat $url";
        } else {
            $url = GCS_URL . "/" . GCP_BUCKET . "/latest-dir";
            $cmd = "$curl -s $url";
        }
    }
    print "$cmd\n" if DEBUG;
    chomp($retval = `$cmd`);
    unless (length($retval)) {
        print STDERR "ERROR: Missing file $url, please try again or report to blast-help\@ncbi.nlm.nih.gov\n";
        exit(EXIT_FAILURE);
    }
    print "$source latest-dir: '$retval'\n" if DEBUG;
    return $retval;
}

# Fetches the JSON text containing the BLASTDB metadata
sub get_blastdb_metadata
{
    my $source = shift;
    my $latest_dir = shift;
    my $ftp = shift;
    my ($url, $cmd);
    my $retval;

    if ($source eq "AWS") {
        $url = AWS_URL . "/" . AWS_BUCKET . "/$latest_dir/" . BLASTDB_METADATA;
        $cmd = "curl -sf $url";
    } elsif ($source eq "GCP") {
        if (defined($gcp_prj)) {
            $url = 'gs://' . GCP_BUCKET . "/$latest_dir/" . BLASTDB_METADATA;
            $cmd = "$gsutil -u $gcp_prj cat $url";
        } else {
            $url = GCS_URL . "/" . GCP_BUCKET . "/$latest_dir/" . BLASTDB_METADATA;
            $cmd = "curl -sf $url";
        }
    } else {
        $url = 'https://' . NCBI_FTP . "/blast/db/" . BLASTDB_METADATA;
        $cmd = "curl -sf $url";
    }
    if (defined $ftp) {
        my $tmpfh = File::Temp->new();
        print "Downloading " . BLASTDB_METADATA . " to $tmpfh\n" if ($opt_verbose > 3);
        $ftp->get(BLASTDB_METADATA, $tmpfh);
        $tmpfh->seek(0, 0);
        $retval = do {
            local $/ = undef;
            <$tmpfh>;
        };
    } else {
        print "$cmd\n" if DEBUG;
        chomp($retval = `$cmd`);
    }
    return ($retval, $url);
}

# Returns the path to the gsutil utility or undef if it is not found
sub get_gsutil_path
{
    return undef if ($^O =~ /mswin/i);
    foreach (qw(/google/google-cloud-sdk/bin /usr/local/bin /usr/bin /snap/bin)) {
        my $path = "$_/gsutil";
        return $path if (-f $path);
    }
    chomp(my $retval = `which gsutil`);
    return $retval if (-f $retval);
    return undef;
}

sub get_gcloud_path
{
    return undef if ($^O =~ /mswin/i);
    foreach (qw(/google/google-cloud-sdk/bin /usr/local/bin /usr/bin /snap/bin)) {
        my $path = "$_/gcloud";
        return $path if (-f $path);
    }
    chomp(my $retval = `which gcloud`);
    return $retval if (-f $retval);
    return undef;
}

sub get_gcp_project
{
    return undef if ($^O =~ /mswin/i);
    my $gcloud = &get_gcloud_path();
    chomp(my $retval = `$gcloud config get-value project`);
    return $retval;
}

# Returns the path to the aws CLI utility or undef if it is not found
sub get_awscli_path
{
    return undef if ($^O =~ /mswin/i);
    foreach (qw(/usr/local/bin /usr/bin)) {
        my $path = "$_/aws";
        return $path if (-f $path);
    }
    chomp(my $retval = `which aws`);
    return $retval if (-f $retval);
    return undef;
}

# Returns the number of cores, assuming lscpu is available, otherwise undef
sub read_lscpu_output 
{
    my $retval;
    if (open(my $fh, "-|", "/usr/bin/lscpu")) {
        my ($num_threads, $num_threads_per_core);
        while (<$fh>) {
            /^CPU.s.:\s*(\d+)/ and $num_threads = $1;
            /^Thread.s. per core:\s*(\d+)/ and $num_threads_per_core = $1;
        }
        if ($num_threads_per_core and $num_threads) {
            $retval = $num_threads / $num_threads_per_core;
        }
        close($fh);
    }
    return $retval;
}

sub compute_default_num_threads
{
    my $retval = 1;
    return $retval if ($^O =~ /mswin/i);
    my $quarter = int(&get_num_cores()*.25);
    return ($quarter > $retval ? $quarter : $retval);
}

# Returns the number of cores based on /proc/cpuinfo, otherwise undef
sub read_proc_cpuinfo 
{
    my $retval;
    if (open(my $fh, "<", "/proc/cpuinfo")) {
        my ($num_siblings, $num_processors, $num_cpu_cores);
        while (<$fh>) {
            /^siblings\s+:\s+(\d+)/ and $num_siblings = $1;
            /^processor/ and $num_processors++;
            /^cpu cores\s+:\s+(\d+)/ and $num_cpu_cores = $1;
        }
        close($fh);
        if ($num_siblings == $num_cpu_cores) {
            $retval = $num_processors;
        } else {
            $retval = $num_processors / ($num_siblings / $num_cpu_cores);
        }
    }
    return $retval;
}

# Returns the number of cores, or 1 if unknown
sub get_num_cores
{
    my $retval = 1;
    if ($^O =~ /linux/i) {
        $retval = &read_lscpu_output();
        unless (defined($retval)) {
            $retval = &read_proc_cpuinfo();
        }
        $retval = 1 unless defined ($retval);
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

sub exponential_delay {
    my $attempt = shift;
    my $delay = 2 ** $attempt;
    return $delay;
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
Supported values: C<ncbi>, C<aws>, or C<gcp>.

=item B<--decompress>

Downloads, decompresses the archives in the current working directory, and
deletes the downloaded archive to save disk space, while preserving the
archive checksum files (default: false).
This is only applicable when the download source is C<ncbi>.

=item B<--showall>

Show all available pre-formatted BLAST databases (default: false). The output
of this option lists the database names which should be used when
requesting downloads or updates using this script.

It accepts the optional arguments: C<tsv> and C<pretty> to produce tab-separated values
and a human-readable format respectively. These parameters elicit the display of
additional metadata if this is available to the program.
This metadata is displayed in columnar format; the columns represent:

name, description, size in gigabytes, date of last update (YYYY-MM-DD format).

=item B<--blastdb_version>

Specify which BLAST database version to download (default: 5).
Supported values: 4, 5

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
On Windows it defaults to 1.
On Linux and macos: defaults to max(1, (number_of_available_cores/4)) and if
set to 0, the script uses all available cores on the machine.

=item B<--legacy_exit_code>

Enables exit codes from prior to version 581818, BLAST+ 2.10.0 release, for
downloads from NCBI only. This option is meant to be used by legacy applications that rely
on this exit codes:
0 for successful operations that result in no downloads, 1 for successful
downloads, and 2 for errors.

=item B<--force_ftp>

Forces downloads using the FTP protocol from the NCBI (Linux and Windows only).
If the location from which to download is not NCBI, this option is ignored.

=item B<--passive>

When using the <force_ftp> option, this flag enables passive FTP, useful when
behind a firewall or working in the cloud (default: true).
To disable passive FTP, configure this option as follows: --passive no

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
