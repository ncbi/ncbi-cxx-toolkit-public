#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

use File::Spec;

use Getopt::Long qw(:config permute no_getopt_compat no_ignore_case);

BEGIN
{
    my $Volume;
    my $NewScriptDir;

    my $ScriptPathname = $0;

    ($Volume, $ScriptDir, $ScriptName) = File::Spec->splitpath($ScriptPathname);

    $ScriptDir = File::Spec->rel2abs(File::Spec->catpath($Volume,
        $ScriptDir, ''));

    while ($ScriptPathname = eval {readlink $ScriptPathname})
    {
        ($Volume, $NewScriptDir) = File::Spec->splitpath($ScriptPathname);

        $ScriptDir = File::Spec->rel2abs(File::Spec->catpath($Volume,
            $NewScriptDir, ''), $ScriptDir);
    }
}

use lib $ScriptDir;

use NCBI::SVN::Update;

sub Usage
{
    my ($ExitCode) = @_;

    die $ScriptName . ' $Revision$' . <<EOF;

This script fixes the officially broken -N switch of the "svn update"
command. That is, it keeps track of which directories have been checked
out non-recursively.

Usage:
    $ScriptName [-l FILE] dir/subdir dir/another/

    $ScriptName

Description:
    This script supports two modes of operation:

    1. With arguments. Arguments are interpreted as pathnames of
       directories to update from the repository. Each path component
       (that is, directory name) that ends with a slash, is checked
       out non-recursively. If a directory pathname does not end with
       a slash, the directory is updated recursively. Otherwise, the
       directory is updated recursively.

    2. Without arguments. In this mode $ScriptName updates the
       current directory the same way it was initially updated.

Options:
    -l          Pathname of the file containing directory names
                to be updated. If this parameter is specified,
                other directory names can be omitted from the
                command line.

EOF

    exit $ExitCode
}

my ($Help, $DirListing);

GetOptions('help|h|?' => \$Help, 'l=s' => \$DirListing)
    or Usage(1);

# Print usage if the --help argument is passed.
Usage(0) if $Help;

my @DirList = @ARGV;

if ($DirListing)
{
    open FILE, '<', $DirListing or die "$ScriptName\: $DirListing\: $!\n";

    while (<FILE>)
    {
        chomp;
        push @DirList, $_ if $_
    }

    close FILE
}

my $Update = NCBI::SVN::Update->new(MyName => $ScriptName);

@DirList ? $Update->UpdateDirList(@DirList) : $Update->UpdateCWD();

exit 0
