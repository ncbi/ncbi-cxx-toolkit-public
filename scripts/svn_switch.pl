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

use NCBI::SVN::MultiSwitch;

use IPC::Open2;

if (@ARGV == 0 || (@ARGV == 1 && $ARGV[0] eq '--help') ||
    (@ARGV == 2 && shift(@ARGV) ne '--switch-map') || @ARGV > 2)
{
    die <<EOF;
Usage:
    $ScriptName [--switch-map] <switch_map_file>

Where:
    switch_map_file     The name of file containing directory mappings.

Description:
    This utility helps to perform multiple switches in a Subversion working
    directory accordingly to the mapping specified by switch_map_file.

EOF
}

die "$ScriptName\: must be in a working copy directory.\n" unless -d '.svn';

NCBI::SVN::MultiSwitch->
    new(MyName => $ScriptName, MapFileName => $ARGV[0])->SwitchUsingMap();

exit 0
