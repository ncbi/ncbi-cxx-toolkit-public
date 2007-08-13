#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

BEGIN
{
    ($ScriptDir, $ScriptName) = $0 =~ m/^(?:(.+)[\\\/])?(.+)$/;
    $ScriptDir ||= '.'
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
    switch_map_file     The name of file containing directory mappings.

Description:
    This utility helps to perform multiple switches in a Subversion working
    directory accordingly to the mapping specified by switch_map_file.

    Each line of a switch map file consists of two whitespace-separated
    parameters for a switch command. The first parameter is a working copy
    directory pathname specified relatively to the current working directory.
    This is the directory that will be switched. The second parameter defines
    a path within the repository (relatively to the repository root) that
    the working copy directory defined by the first parameter will be
    switched to.

    Note that all other "switched" directories under the current working
    copy directory will be "unswitched" to their original location.

EOF
}

die "$ScriptName\: must be in a working copy directory.\n" unless -d '.svn';

NCBI::SVN::MultiSwitch->new(MyName => $ScriptName)->SwitchUsingMap(
    NCBI::SVN::SwitchMap->new(MyName => $ScriptName, MapFileName => $ARGV[0])->
        GetSwitchPlan());

exit 0
