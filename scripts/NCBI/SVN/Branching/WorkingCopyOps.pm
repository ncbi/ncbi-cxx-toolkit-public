# $Id$

package NCBI::SVN::Branching::WorkingCopyOps;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::Util;
use NCBI::SVN::Branching::BranchInfo;
use NCBI::SVN::Branching::WorkingCopyInfo;

sub DoUpdateAndSwitch
{
    my ($Self, $WorkingCopyInfo, $TargetPath,
        $PathsToUpdate, $PathsToSwitch) = @_;

    my ($RootURL, $MissingTree, $MissingBranchPaths) =
        @$WorkingCopyInfo{qw(RootURL MissingTree MissingBranchPaths)};

    $Self->{SVN}->RunSubversion('update', '-N', @$MissingTree) if @$MissingTree;

    my @PathsToUpdate = (@$PathsToUpdate, @$MissingBranchPaths);

    $Self->{SVN}->RunSubversion('update', @PathsToUpdate) if @PathsToUpdate;

    my $BaseURL = "$RootURL/$TargetPath/";

    for (@$PathsToSwitch, @$MissingBranchPaths,
        @{$WorkingCopyInfo->{IncorrectlySwitched}})
    {
        $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
    }

    $BaseURL = "$RootURL/$WorkingCopyInfo->{WorkingDirPath}/";

    for (@{$WorkingCopyInfo->{ObsoletePaths}})
    {
        $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
    }
}

sub Switch
{
    my ($Self, $BranchPath, $Parent) = @_;

    my $WorkingCopyInfo =
        NCBI::SVN::Branching::WorkingCopyInfo->new($BranchPath);

    my $TargetPath = $Parent ?
        $WorkingCopyInfo->{BranchInfo}->{UpstreamPath} : $BranchPath;

    print STDERR "Switching to '$TargetPath'...\n";

    $Self->DoUpdateAndSwitch($WorkingCopyInfo, $TargetPath,
        @$WorkingCopyInfo{$Parent ? qw(SwitchedToParent SwitchedToBranch) :
            qw(SwitchedToBranch SwitchedToParent)});

    return $WorkingCopyInfo
}

sub Update
{
    my ($Self, $BranchPath) = @_;

    my $WorkingCopyInfo =
        NCBI::SVN::Branching::WorkingCopyInfo->new($BranchPath);

    my ($SwitchedToBranch, $SwitchedToParent) =
        @$WorkingCopyInfo{qw(SwitchedToBranch SwitchedToParent)};

    my $TargetPath;
    my $PathsToUpdate;
    my $PathsToSwitch;

    if (@$SwitchedToBranch)
    {
        $TargetPath = $BranchPath;
        $PathsToUpdate = $SwitchedToBranch;
        $PathsToSwitch = $SwitchedToParent
    }
    elsif (@$SwitchedToParent)
    {
        $TargetPath = $WorkingCopyInfo->{BranchInfo}->{UpstreamPath};
        $PathsToUpdate = $SwitchedToParent;
        $PathsToSwitch = []
    }
    else
    {
        die "$Self->{MyName}: unable to find a check-out of '$BranchPath'.\n"
    }

    print STDERR "Updating working copy directories of '$BranchPath'...\n";

    $Self->DoUpdateAndSwitch($WorkingCopyInfo, $TargetPath,
        $PathsToUpdate, $PathsToSwitch)
}

sub SetRawMergeProp
{
    my ($Self, $BranchPath, $Changes, @BranchPaths) = @_;

    $Self->{SVN}->RunSubversion('propset', '-R', 'ncbi:raw',
        "Please run '$Self->{MyName} commit_merge " .
        NCBI::SVN::Branching::Util::SimplifyBranchPath($BranchPath) .
        "' to merge $Changes", @BranchPaths)
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $SourceRev) = @_;

    my $WorkingCopyInfo = $Self->Switch($BranchPath, $Direction eq 'up');

    my ($BranchInfo, $RootURL) = @$WorkingCopyInfo{qw(BranchInfo RootURL)};

    my $UpstreamInfo = NCBI::SVN::Branching::UpstreamInfo->new($BranchInfo);

    my @BranchPaths = @{$BranchInfo->{BranchPaths}};

    print STDERR "Running svn status on branch directories...\n";

    for ($Self->{SVN}->ReadSubversionLines('status', @BranchPaths))
    {
        die "$Self->{MyName}: local modifications detected.\n"
            unless m/^(?:[\?~X]|    S)/o
    }

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    my ($SourcePath, $TargetPath,
        $LastSourceRev, $LastTargetRev, $SourceRevisions) =
        $Direction eq 'up' ?
            ($BranchPath, $UpstreamPath,
                $UpstreamInfo->{LastUpSyncRevisionNumber},
                $BranchInfo->{LastDownSyncRevisionNumber},
                $BranchInfo->{BranchRevisions}) :
            ($UpstreamPath, $BranchPath,
                $BranchInfo->{LastDownSyncRevisionNumber},
                $UpstreamInfo->{LastUpSyncRevisionNumber},
                $UpstreamInfo->{UpstreamRevisions});

    $SourceRev ||= $SourceRevisions->[0]->{Number} || 0;

    unless ($SourceRev > $LastSourceRev)
    {
        die "$Self->{MyName}: '$TargetPath' is already " .
            "in sync with r$LastSourceRev of '$SourcePath'\n"
    }

    my $SourceURL = $RootURL . '/' . $SourcePath . '/';
    my $TargetURL = $RootURL . '/' . $TargetPath . '/';

    print STDERR "Merging with r$SourceRev...\n";

    my $LastMergeRevision = $UpstreamInfo->{MergeRevisions}->[0];

    my $BranchPathInfo = $BranchInfo->{BranchPathInfo};

    if ($LastMergeRevision and
        $LastMergeRevision->{MergeDirection} ne $Direction)
    {
        # Previous merge was of the opposite direction.
        for my $Path (@BranchPaths)
        {
            my $PathInfo = $BranchPathInfo->{$Path};

            my $PathCreationRevisionNumber =
                $PathInfo->{PathCreationRevision}->{Number};

            my $PathSourceRevisionNumber =
                $PathInfo->{SourceRevisionNumber};

            my ($MinTargetRev, $MinSourceRev) = $Direction eq 'up' ?
                ($PathSourceRevisionNumber, $PathCreationRevisionNumber) :
                ($PathCreationRevisionNumber, $PathSourceRevisionNumber);

            my $LocalLastTargetRev =
                $LastTargetRev > $MinTargetRev ? $LastTargetRev : $MinTargetRev;

            my $LocalSourceRev =
                $SourceRev > $MinSourceRev ? $SourceRev : $MinSourceRev;

            if ($LocalLastTargetRev < $LocalSourceRev)
            {
                $Self->{SVN}->RunSubversion('merge',
                    $TargetURL . $Path . '@' . $LocalLastTargetRev,
                    $SourceURL . $Path . '@' . $LocalSourceRev,
                    $Path)
            }
        }
    }
    else
    {
        # Either there were no merge revisions or
        # previous merge was of the same direction.
        for my $Path (@BranchPaths)
        {
            my $PathInfo = $BranchPathInfo->{$Path};

            my $MinSourceRev = $Direction eq 'up' ?
                $PathInfo->{PathCreationRevision}->{Number} :
                    $PathInfo->{SourceRevisionNumber};

            my $LocalLastSourceRev =
                $LastSourceRev > $MinSourceRev ? $LastSourceRev : $MinSourceRev;

            if ($LocalLastSourceRev < $SourceRev)
            {
                $Self->{SVN}->RunSubversion('merge',
                    "-r$LocalLastSourceRev\:$SourceRev",
                        $SourceURL . $Path, $Path)
            }
        }
    }

    $Self->SetRawMergeProp($BranchPath,
        NCBI::SVN::Branching::Util::MakeMergeLogMessage($SourcePath,
            $TargetPath, $LastTargetRev, $SourceRev), @BranchPaths)
}

sub MergeDownInto
{
    my $Self = shift;

    return $Self->DoMerge('down', @_)
}

sub MergeUpFrom
{
    my $Self = shift;

    return $Self->DoMerge('up', @_)
}

sub CommitMerge
{
    my ($Self, $BranchPath, $MergeDescription) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

    my @BranchPaths = @{$BranchInfo->{BranchPaths}};

    my $Message;

    for my $Path (@BranchPaths)
    {
        my $PropValue =
            $Self->{SVN}->ReadSubversionStream('propget', 'ncbi:raw', $Path);

        unless ($Message)
        {
            $Message = $PropValue
        }
        elsif ($Message ne $PropValue)
        {
            die "$Self->{MyName}: wrong cwd for the commit_merge operation.\n"
        }
    }

    my ($SourcePath, $TargetPath, $FromRevisionNumber, $ToRevisionNumber) =
        NCBI::SVN::Branching::Util::ParseMergeLogMessage($Message) or
            die "$Self->{MyName}: cannot retrieve log message.\n";

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    if ($SourcePath eq $BranchPath)
    {
        die "$Self->{MyName}: wrong BRANCH_PATH argument for this merge.\n"
            if $TargetPath ne $UpstreamPath;

        die "$Self->{MyName}: upstream merge requires a log message.\n"
            unless $MergeDescription
    }
    elsif ($TargetPath ne $BranchPath || $SourcePath ne $UpstreamPath)
    {
        die "$Self->{MyName}: wrong BRANCH_PATH argument for this merge.\n"
    }

    $Self->{SVN}->RunSubversion('propdel', '-R', 'ncbi:raw', @BranchPaths);

    my $LogMessage = NCBI::SVN::Branching::Util::MakeMergeLogMessage(
        $SourcePath, $TargetPath, $FromRevisionNumber,
            $ToRevisionNumber, $MergeDescription);

    eval
    {
        $Self->{SVN}->RunSubversion('commit', '-m', 'Merged ' .
            $LogMessage, @BranchPaths)
    };

    $Self->SetRawMergeProp($BranchPath, $LogMessage, @BranchPaths) if $@
}

sub MergeDiff
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('diff',
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchPaths}});

    my $Line;
    my $State = 0;
    my $Buffer;

    while (defined($Line = $Stream->ReadLine()))
    {
        if ($State == 0)
        {
            if ($Line =~ m/^[\r\n]*$/o)
            {
                $Buffer = $Line;
                $State = 1;
                next
            }
        }
        elsif ($State == 1)
        {
            if ($Line =~ m/^Property changes on: /o)
            {
                $Buffer .= $Line;
                $Line = $Stream->ReadLine();
                if ($Line =~ m/^_{66}/o)
                {
                    $Buffer .= $Line;
                    $State = 2;
                    next
                }
            }
        }
        elsif ($Line =~ m/^Name: ncbi:raw/o)
        {
            $Stream->ReadLine();
            next
        }
        elsif ($Line =~ m/^[\r\n]*$/o)
        {
            $State = 0;
            next
        }
        else
        {
            print $Buffer;
            $State = 0
        }

        print $Line
    }
}

sub MergeStat
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('status',
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchPaths}});

    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        next if $Line =~ s/^(.)M/$1 /o && $Line =~ m/^ {7}/o;

        print $Line
    }

    print "\nNOTE:  all property changes were omitted; " .
        "status may be inaccurate\n"
}

sub Svn
{
    my ($Self, $BranchPath, @CommandAndArgs) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    exec($Self->{SVN}->GetSvnPathname(), @CommandAndArgs,
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchPaths}})
}

1
