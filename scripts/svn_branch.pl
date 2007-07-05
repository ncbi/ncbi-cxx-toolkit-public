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

use Getopt::Long qw(:config permute no_getopt_compat no_ignore_case);

sub Help
{
    my ($Topic) = @_;

    print <<EOF;
Usage:
    $ScriptName list

    $ScriptName info <branch_path>

    $ScriptName create <branch_path> <upstream_path>[:<rev>] <branch_dirs>

    $ScriptName alter <branch_path> <branch_dirs>

    $ScriptName remove <branch_path>

    $ScriptName merge_down <branch_path> [<trunk_rev>]

    $ScriptName merge_down_commit <branch_path>

    $ScriptName merge_up <branch_path> [<branch_rev>]

    $ScriptName merge_up_commit <branch_path>

    $ScriptName svn <branch_path> <svn_command_with_args>

    $ScriptName switch <branch_path>

    $ScriptName unswitch <branch_path>

Where:
    branch_path         Path in the repository (relative to /branches)
                        that identifies the branch.

    trunk_rev           Trunk revision to operate with (HEAD by default).

Commands:
    list                Prints the list of branches that were created
                        with the help of this utility.

    info                Displays brief information about the branch
                        specified by the <branch_path> argument.

    create              Creates a new branch defined by its base path
                        <branch_path>, upstream path, and directory
                        listing <branch_dirs>.

    alter               Modifies branch structure bringing it to the state
                        as if it was initially created with directory
                        listing <branch_dirs>.

    remove              Removes branch identified by the path <branch_path>
                        from the repository.

    merge_down          Merges the latest changes from the trunk into the
                        branch identified by <branch_path>. The branch must
                        be checked out to the current working directory and
                        there must be no local changes in it.

    merge_up            Merges the latest changes from the branch back to the
                        trunk. The trunk stream must be checked out to the
                        current working directory and there must be no local
                        changes in it.

    commit_merge        Commits the results of merge_down or merge_up.
                        A special format of the commit log message is used.

    svn                 Executes an arbitrary Subversion command with branch
                        directory names as its arguments.

    switch              Switches working copy directories to the branch
                        identified by <branch_path>.

    unswitch            "Unswitches" working copy directories from the
                        branch identified by <branch_path>.

Description:
    This script facilitates branch handling using Subversion.

EOF

    exit 0
}

sub UsageError
{
    my ($Error) = @_;

    print STDERR ($Error ? "$ScriptName\: $Error\n" : '') .
        "Type '$ScriptName help' for usage.\n";

    exit 1
}

sub CheckNumberOfArguments
{
    my ($Command, $NumReqArgs) = @_;

    if (@ARGV != $NumReqArgs)
    {
        UsageError($NumReqArgs == 0 ? qq("$Command" doesn't accept arguments) :
            $NumReqArgs == 1 ? qq("$Command" accepts only one argument) :
            qq("$Command" requires $NumReqArgs arguments))
    }
}

my $DirList;

GetOptions('help|h|?' => sub {Help()}, 'dirlist' => \$DirList) or UsageError();

my $Command = shift @ARGV;

unless (defined $Command)
{
    UsageError()
}
elsif ($Command eq 'help')
{
    CheckNumberOfArguments('help', 1);

    Help(@ARGV)
}

sub AdjustBranchPath
{
    my ($BranchPath) = @_;

    return $BranchPath !~ m/^branches\// ?
        'branches/' . $BranchPath : $BranchPath
}

sub CheckBranchStructureCommandArgs
{
    my ($Command, $NumReqArgs) = @_;

    if (@ARGV < $NumReqArgs || (@ARGV == $NumReqArgs && !$DirList))
    {
        UsageError("$Command requires $NumReqArgs arguments, " .
            'followed by a directory listing')
    }

    if (@ARGV > $NumReqArgs && $DirList)
    {
        UsageError('excessive command line arguments')
    }

    return @ARGV unless $DirList;

    my @BranchDirs;

    if ($DirList eq '-')
    {
        while (<STDIN>)
        {
            chomp;
            push @BranchDirs, $_
        }
    }
    else
    {
        open FILE, '<', $DirList or die "$ScriptName\: $DirList\: $!\n";

        while (<FILE>)
        {
            chomp;
            push @BranchDirs, $_
        }

        close FILE
    }

    return (@ARGV, @BranchDirs)
}

my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);

if ($Command eq 'list')
{
    CheckNumberOfArguments('list', 0);

    $Module->List()
}
elsif ($Command eq 'info')
{
    CheckNumberOfArguments('info', 1);

    $Module->Info(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'create')
{
    $Module->Create(CheckBranchStructureCommandArgs('create', 2))
}
elsif ($Command eq 'alter')
{
    $Module->Alter(CheckBranchStructureCommandArgs('alter', 1))
}
elsif ($Command eq 'remove')
{
    CheckNumberOfArguments('remove', 1);

    $Module->Remove(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'merge_down')
{
    CheckNumberOfArguments('merge_down', 1);

    $Module->MergeDown(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'merge_up')
{
    CheckNumberOfArguments('merge_up', 1);

    $Module->MergeUp(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'commit_merge')
{
    CheckNumberOfArguments('commit_merge', 1);

    $Module->CommitMerge(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'switch')
{
    CheckNumberOfArguments('switch', 1);

    $Module->Switch(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'unswitch')
{
    CheckNumberOfArguments('unswitch', 1);

    $Module->Unswitch(AdjustBranchPath(@ARGV))
}
elsif ($Command eq 'svn')
{
    $Module->Svn(@ARGV)
}
else
{
    UsageError("unknown command '$Command'")
}

exit 0
