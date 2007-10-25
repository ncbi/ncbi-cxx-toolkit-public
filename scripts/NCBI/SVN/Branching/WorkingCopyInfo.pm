# $Id$

package NCBI::SVN::Branching::WorkingCopyInfo;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::BranchInfo;
use NCBI::SVN::Branching::Util;

sub new
{
    my ($Class, $BranchPath) = @_;

    my @SwitchedToBranch;
    my @SwitchedToParent;
    my @IncorrectlySwitched;
    my @MissingBranchPaths;
    my @MissingTree;
    my @ObsoletePaths;

    my $Self = $Class->SUPER::new(
        SwitchedToBranch => \@SwitchedToBranch,
        SwitchedToParent => \@SwitchedToParent,
        IncorrectlySwitched => \@IncorrectlySwitched,
        MissingBranchPaths => \@MissingBranchPaths,
        MissingTree => \@MissingTree,
        ObsoletePaths => \@ObsoletePaths
    );

    my $WorkingDirInfo = $Self->{SVN}->ReadInfo('.')->{'.'};

    $Self->{WorkingDirPath} = $WorkingDirInfo->{Path};

    my $RootURL = $Self->{RootURL} =
        $WorkingDirInfo && $WorkingDirInfo->{Root} ||
            die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo = $Self->{BranchInfo} =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

    my ($BranchPaths, $UpstreamPath, $ObsoleteBranchPaths) =
        @$BranchInfo{qw(BranchPaths UpstreamPath ObsoleteBranchPaths)};

    my @ExistingBranchPaths;

    for my $Path (@$BranchPaths)
    {
        if (-e $Path)
        {
            push @ExistingBranchPaths, $Path
        }
        else
        {
            push @MissingBranchPaths, $Path
        }
    }

    my @ExistingObsoletePaths;

    if ($ObsoleteBranchPaths)
    {
        for my $Path (reverse @$ObsoleteBranchPaths)
        {
            push @ExistingObsoletePaths, $Path if -e $Path
        }
    }

    my $ExistingPathInfo =
        $Self->{SVN}->ReadInfo(@ExistingBranchPaths, @ExistingObsoletePaths);

    for my $Path (@ExistingBranchPaths)
    {
        my $Info = delete $ExistingPathInfo->{$Path};

        unless ($Info)
        {
            push @MissingBranchPaths, $Path
        }
        elsif ($Info->{Path} eq "$BranchPath/$Path")
        {
            push @SwitchedToBranch, $Path
        }
        elsif ($Info->{Path} eq "$UpstreamPath/$Path")
        {
            push @SwitchedToParent, $Path
        }
        else
        {
            push @IncorrectlySwitched, $Path
        }
    }

    if (@MissingBranchPaths)
    {
        my %MissingTree;

        for my $MissingPath (@MissingBranchPaths)
        {
            my $Path = '.';
            my $PathStillExists = 1;
            my $Tree = \%MissingTree;

            my @PathComponents = split('/', $MissingPath);
            pop @PathComponents;

            for my $PathComponent (@PathComponents)
            {
                $Tree = ($Tree->{$PathComponent} ||= {});

                $Tree->{'/'} = 1 unless $PathStillExists &&=
                    -e ($Path .= '/' . $PathComponent)
            }
        }

        @MissingTree =
            sort @{NCBI::SVN::Branching::Util::FindPathsInTree(\%MissingTree)}
    }

    while (my ($Path, $Info) = each %$ExistingPathInfo)
    {
        if ($Info->{Path} eq "$BranchPath/$Path")
        {
            push @ObsoletePaths, $Path
        }
    }

    return $Self
}

1
