#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

use File::Spec;

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

use NCBI::SVN::SwitchMap;
use NCBI::SVN::MultiSwitch;

if (@ARGV != 1 || $ARGV[0] eq '--help')
{
    die <<EOF;
Usage:
    $ScriptName <switch_map_file>

Where:
    switch_map_file  -- file with directory mappings

Description:
    This utility helps to perform multiple switches in a Subversion working
    directory according to the mapping specified by switch_map_file.

    Each line of the switch_map_file consists of two whitespace-separated
    arguments for a 'svn switch' command.
    The first argument is a working copy directory pathname specified
    relatively to the current working directory. This is the directory
    that will be switched.
    The second argument defines a path within the repository (relative
    to the repository root) that the working copy directory defined by
    the first argument will be switched to.

    NOTE:  All "switched" directories under the current working directory
           which are not listed in the switch_map_file will be switched
           back to their original locations.

EOF
}

die "$ScriptName\: must be in a working copy directory.\n" unless -d '.svn';

NCBI::SVN::MultiSwitch->new(MyName => $ScriptName)->SwitchUsingMap(
    NCBI::SVN::SwitchMap->new(MyName => $ScriptName, MapFileName => $ARGV[0])->
        GetSwitchPlan());

exit 0
