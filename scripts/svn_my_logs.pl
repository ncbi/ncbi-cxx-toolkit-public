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

use NCBI::SVN::Wrapper;
use XML::Parser;

sub Usage
{
    my ($Stream, $Status) = @_;

    print $Stream <<EOF;
Usage:
    $ScriptName <begin_date> <end_date>

Where:
    begin_date      The date of the first log message in a format accepted
                    by the Subversion log command.

    end_date        The date PAST the last log message you want to receive.

Description:
    This script dumps Subversion log messages for revisions committed
    by the effective user during the specified period.

EOF

    exit $Status
}

if (!@ARGV || $ARGV[0] =~ m/help/)
{
    Usage(\*STDOUT, 0)
}
elsif (@ARGV != 2)
{
    Usage(\*STDERR, 1)
}

my ($BeginDate, $EndDate) = @ARGV;

my $UserName = $ENV{USER};

my $SVN = NCBI::SVN::Wrapper->new(MyName => $ScriptName);

my $Log = $SVN->ReadSubversionStream('log', '--non-interactive', '--xml',
    "-r{$ARGV[0]}:{$ARGV[1]}", $SVN->GetRootURL() ||
        die "$ScriptName\: not in a working copy.\n");

my $Parser = new XML::Parser(Style => 'Tree');

my $LogEntries = $Parser->parse($Log);

die if shift(@$LogEntries) ne 'log';

$LogEntries = shift(@$LogEntries);

die if ref(shift(@$LogEntries)) ne 'HASH';

my $NextIsLogEntry;

for my $LogEntry (@$LogEntries)
{
    if ($NextIsLogEntry)
    {
        undef $NextIsLogEntry;

        my $Revision = shift(@$LogEntry)->{revision};

        my %LogEntry = @$LogEntry;
        delete $LogEntry{0};

        for my $Value (values %LogEntry)
        {
            die if $Value->[1] ne '0';
            $Value = $Value->[2]
        }

        print "$LogEntry{msg} [r$Revision, $LogEntry{date}]\n\n"
            if $LogEntry{author} eq $UserName
    }
    elsif (!ref($LogEntry) && $LogEntry eq 'logentry')
    {
        $NextIsLogEntry = 1
    }
}

exit 0
