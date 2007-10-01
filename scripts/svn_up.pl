#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

use File::Spec;

BEGIN
{
    my $ScriptPathname = $0;

    ($ScriptDir, $ScriptName) = $ScriptPathname =~ m/^(?:(.+)[\\\/])?(.+)$/;

    $ScriptDir = File::Spec->rel2abs($ScriptDir || '.');

    while ($ScriptPathname = eval {readlink $ScriptPathname})
    {
        my ($NewScriptDir) = $ScriptPathname =~ m/^(?:(.+)[\\\/])?/;

        $ScriptDir = File::Spec->rel2abs($NewScriptDir || '.', $ScriptDir)
    }
}

use lib $ScriptDir;

use NCBI::SVN::Update;

# Print usage if the --help argument is passed.
if (@ARGV && $ARGV[0] eq '--help')
{
    die $ScriptName . ' $Revision$' . <<EOF;

This script fixes the officially broken -N switch of the "svn update"
command. That is, it keeps track of which directories have been checked
out non-recursively.

Usage:
    $ScriptName [-r REV] dir/subdir dir/another/

    $ScriptName [-r REV]

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

EOF
}

my $Update = NCBI::SVN::Update->new(MyName => $ScriptName);

my $Rev;

if (@ARGV && $ARGV[0] =~ m/^-r(.*)/)
{
    shift @ARGV;
    $Rev = $1 ? $1 : shift @ARGV
}

@ARGV ? $Update->UpdateDirList($Rev, @ARGV) : $Update->UpdateCWD($Rev);

exit 0
