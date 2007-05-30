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

use NCBI::SVN::Branching;

sub Usage
{
    my ($Stream, $Status) = @_;

    print $Stream <<EOF;
Usage:
    $ScriptName create <branch_path> <branch_map_file> [<trunk_rev>]

    $ScriptName remove <branch_path>

    $ScriptName merge_down <branch_path> [<trunk_rev>]

    $ScriptName merge_down_commit <branch_path>

    $ScriptName commit <branch_path> [<log_message>]

Where:
    branch_map_file     Pathname of the file containing branch directory
                        mappings.

    branch_path         Path in the repository (relative to /branches)
                        that identifies the branch.

    trunk_rev           Trunk revision to operate with (HEAD by default).

Commands:
    create              Either creates a new branch defined by its base path
                        <branch_path> and directory mapping <branch_map_file>
                        or updates (accordingly to the new directory mapping
                        <branch_map_file>) an existing branch identified by
                        its base path <branch_path>.

    remove              Removes branch identified by the path <branch_path>
                        from the repository.

    merge_down          Merges the latest changes from the trunk into the
                        branch identified by <branch_path>. The branch must
                        be checked out to the current working directory and
                        there must be no local changes in it.

    merge_down_commit   Commits the results of merge to the branch. A special
                        format of the commit log message will be used.

    commit              Commits the local changes to the branch. If the log
                        message is omitted, it is read from the standard input.

Description:
    This script facilitates branch handling using Subversion.

EOF

    exit $Status
}

my ($Command, @Params) = @ARGV;

my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);

unless (defined $Command)
{
    Usage(\*STDERR, 1)
}
elsif ($Command eq '--help' || $Command eq '-h')
{
    Usage(\*STDOUT, 0)
}
elsif ($Command eq 'create')
{
    $Module->Create(@Params)
}
elsif ($Command eq 'remove')
{
    $Module->Remove(@Params)
}
elsif ($Command eq 'merge_down')
{
    $Module->MergeDown(@Params)
}
elsif ($Command eq 'merge_down_commit')
{
    $Module->MergeDownCommit(@Params)
}
elsif ($Command eq 'commit')
{
    $Module->Commit(@Params)
}
else
{
    die "$ScriptName\: unknown command '$Command'\n"
}

exit 0
