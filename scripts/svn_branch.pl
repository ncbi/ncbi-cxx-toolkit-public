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
    $ScriptName list

    $ScriptName create <branch_path> <branch_map_file> [<trunk_rev>]

    $ScriptName remove <branch_path>

    $ScriptName merge_down <branch_path> [<trunk_rev>]

    $ScriptName merge_down_commit <branch_path>

    $ScriptName commit <branch_path> [<log_message>]

    $ScriptName switch <branch_path>

Where:
    branch_map_file     Pathname of the file containing branch directory
                        mappings.

    branch_path         Path in the repository (relative to /branches)
                        that identifies the branch.

    trunk_rev           Trunk revision to operate with (HEAD by default).

Commands:
    list                Prints the list of branches that were created
                        with the help of this utility.

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

    switch              Switch working copy directories to the branch
                        identified by <branch_path>.

Description:
    This script facilitates branch handling using Subversion.

EOF

    exit $Status
}

my ($Command, @Params) = @ARGV;

my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);
my $Method;

unless (defined $Command)
{
    Usage(\*STDERR, 1)
}
elsif ($Command eq '--help' || $Command eq '-h')
{
    Usage(\*STDOUT, 0)
}
elsif ($Command eq 'list')
{
    $Method = 'List'
}
elsif ($Command eq 'create')
{
    $Method = 'Create'
}
elsif ($Command eq 'remove')
{
    $Method = 'Remove'
}
elsif ($Command eq 'merge_down')
{
    $Method = 'MergeDown'
}
elsif ($Command eq 'merge_down_commit')
{
    $Method = 'MergeDownCommit'
}
elsif ($Command eq 'commit')
{
    $Method = 'Commit'
}
elsif ($Command eq 'switch')
{
    $Method = 'Switch'
}
else
{
    die "$ScriptName\: unknown command '$Command'\n"
}

$Module->$Method(@Params);

exit 0
