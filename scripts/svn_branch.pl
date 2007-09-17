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

    $ScriptName update <branch_path>

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

    update              Updates working copy directories of the branch
                        identified by <branch_path>.

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

sub TooManyArgs
{
    my ($Command) = @_;

    UsageError(qq(too many arguments for "$Command"))
}

my ($Force, $RootURL, $Revision, $PathList);

GetOptions('help|h|?' => sub {Help()},
    'force|f' => \$Force,
    'root-url|root|U=s' => \$RootURL,
    'revision|rev|r=i' => \$Revision,
    'list|l=s' => \$PathList)
        or UsageError();

my $Module = NCBI::SVN::Branching->new(MyName => $ScriptName);

my $Command = shift @ARGV;

unless (defined $Command)
{
    UsageError()
}
elsif ($Command eq 'help')
{
    TooManyArgs($Command) if @ARGV > 1;

    Help($ARGV[0])
}

sub TrimSlashes
{
    my ($PathRef) = @_;

    $$PathRef =~ s/\/+$//o;
    $$PathRef =~ s/^\/+//o
}

sub AdjustBranchPath
{
    my ($BranchPath) = @_;

    unless ($BranchPath =~ m/^(?:\/|trunk|branches)/)
    {
        $BranchPath = 'branches/' . $BranchPath
    }

    TrimSlashes(\$BranchPath);

    return $BranchPath
}

sub ExtractBranchPathArg
{
    my ($ArgName) = @_;

    return AdjustBranchPath(shift(@ARGV) or
        UsageError('"' . ($ArgName || 'branch_path') .
            '" argument is missing'))
}

sub RequireRootURL
{
    return $RootURL || $Module->{SVN}->GetRootURL() ||
        die "$ScriptName\: chdir to a working copy or use --root-url\n"
}

sub AcceptOnlyBranchPathArg
{
    my ($Command) = @_;

    my $BranchPath = ExtractBranchPathArg();

    TooManyArgs($Command) if @ARGV;

    return $BranchPath
}

sub ReadFileLines
{
    my ($FH) = @_;

    my $Line;
    my @Lines;

    while (defined($Line = readline($FH)))
    {
        chomp $Line;
        next unless $Line;
        push @Lines, $Line
    }

    return @Lines
}

sub GetBranchDirArgs
{
    my ($Command, @AdditionalPathArgs) = @_;

    my @BranchPaths;

    unless ($PathList)
    {
        UsageError('directory listing is missing') unless @ARGV;

        @BranchPaths = @ARGV
    }
    else
    {
        TooManyArgs($Command) if @ARGV;

        if ($PathList eq '-')
        {
            @BranchPaths = ReadFileLines(\*STDIN)
        }
        else
        {
            open FILE, '<', $PathList or die "$ScriptName\: $PathList\: $!\n";

            @BranchPaths = ReadFileLines(\*FILE);

            close FILE
        }
    }

    for my $Path (@BranchPaths)
    {
        TrimSlashes(\$Path)
    }

    return (@AdditionalPathArgs, @BranchPaths)
}

if ($Command eq 'list')
{
    UsageError(q("list" doesn't accept arguments)) if @ARGV;

    $Module->List(RequireRootURL())
}
elsif ($Command eq 'info')
{
    $Module->Info(RequireRootURL(), AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'create')
{
    my $BranchPath = ExtractBranchPathArg();
    my $UpstreamPath = ExtractBranchPathArg('upstream_path');

    $Module->Create(RequireRootURL(), $BranchPath,
        $UpstreamPath, $Revision, GetBranchDirArgs($Command))
}
elsif ($Command eq 'alter')
{
    my $BranchPath = ExtractBranchPathArg();

    $Module->Alter($Force, RequireRootURL(), $BranchPath,
        GetBranchDirArgs($Command))
}
elsif ($Command eq 'remove')
{
    $Module->Remove($Force, RequireRootURL(), AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_down')
{
    $Module->MergeDown(AcceptOnlyBranchPathArg($Command), $Revision)
}
elsif ($Command eq 'merge_up')
{
    $Module->MergeUp(AcceptOnlyBranchPathArg($Command), $Revision)
}
elsif ($Command eq 'commit_merge')
{
    $Module->CommitMerge(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_diff')
{
    $Module->MergeDiff(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'merge_stat')
{
    $Module->MergeStat(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'update')
{
    $Module->Update(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'switch')
{
    $Module->Switch(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'unswitch')
{
    $Module->Unswitch(AcceptOnlyBranchPathArg($Command))
}
elsif ($Command eq 'svn')
{
    UsageError('not enough arguments for "svn"') if @ARGV < 2;

    my $BranchPath = AdjustBranchPath(pop @ARGV);

    $Module->Svn($BranchPath, @ARGV)
}
else
{
    UsageError("unknown command '$Command'")
}

exit 0
