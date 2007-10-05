# $Id$

package NCBI::SVN::Branching::BranchInfo;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::Util;

sub SetUpstreamAndDownSynchRev
{
    my ($Self, $SourcePath, $BranchDir, $SourceRevisionNumber, $Revision) = @_;

    my $UpstreamPath = $SourcePath;

    length($UpstreamPath) > length($BranchDir) and
        substr($UpstreamPath, -length($BranchDir),
            length($BranchDir), '') eq $BranchDir
        or die 'branch structure does not replicate ' .
            "upstream structure in r$Revision->{Number}";

    unless (my $SameUpstreamPath = $Self->{UpstreamPath})
    {
        $Self->{UpstreamPath} = $UpstreamPath
    }
    else
    {
        die "inconsistent source paths in r$Revision->{Number}"
            if $UpstreamPath ne $SameUpstreamPath
    }

    $Self->{LastDownSyncRevisionNumber} ||= $SourceRevisionNumber
}

sub MergeDeletedTrees
{
    my ($ObsoletePathTree, $Subdir, $DeletedNode) = @_;

    if (exists $DeletedNode->{'/'})
    {
        $ObsoletePathTree->{$Subdir} = {'/' => 1};

        return
    }

    $ObsoletePathTree = ($ObsoletePathTree->{$Subdir} ||= {});

    unless (exists $ObsoletePathTree->{'/'})
    {
        while (my ($Subdir, $Subtree) = each %$DeletedNode)
        {
            MergeDeletedTrees($ObsoletePathTree, $Subdir, $Subtree)
        }
    }

    return 0
}

sub ClearDeletedTree
{
    my ($ObsoletePathTree, $BranchStructure) = @_;

    while (my ($Subdir, $Subtree) = each %$BranchStructure)
    {
        if ($Subtree->{'/'})
        {
            delete $ObsoletePathTree->{$Subdir};

            next
        }

        if (my $ObsoletePathSubtree = $ObsoletePathTree->{$Subdir})
        {
            ClearDeletedTree($ObsoletePathSubtree, $Subtree)
        }
    }

    return 0
}

sub ModelBranchStructure
{
    my ($Self, $BranchStructure, $Revision, $CommonTarget) = @_;

    for my $Change (@{$Revision->{ChangedPaths}})
    {
        my ($ChangeType, $BranchDir,
            $SourcePath, $SourceRevisionNumber) = @$Change;

        next if substr($BranchDir, 0, length($CommonTarget), '')
            ne $CommonTarget;

        if ($ChangeType eq 'D' || $ChangeType eq 'R')
        {
            my $ParentNode = $BranchStructure;

            my @Subdirs = split('/', $BranchDir);
            my $Name = pop(@Subdirs);

            for my $Subdir (@Subdirs)
            {
                $ParentNode = ($ParentNode->{$Subdir} ||= {})
            }

            if ($ChangeType eq 'R' && $SourcePath)
            {
                $ParentNode->{$Name} = {'/' => 1};

                $Self->SetUpstreamAndDownSynchRev($SourcePath,
                    $BranchDir, $SourceRevisionNumber)
            }
            elsif (my $DeletedNode = delete $ParentNode->{$Name})
            {
                my $ObsoletePathTree = ($Self->{ObsoleteBranchPaths} ||= {});

                for my $Subdir (@Subdirs)
                {
                    $ObsoletePathTree = ($ObsoletePathTree->{$Subdir} ||= {})
                }

                MergeDeletedTrees($ObsoletePathTree, $Name, $DeletedNode)
            }
        }
        elsif ($ChangeType eq 'A' && $SourcePath)
        {
            NCBI::SVN::Branching::Util::FindTreeNode($BranchStructure,
                $BranchDir)->{'/'} = 1;

            $Self->SetUpstreamAndDownSynchRev($SourcePath,
                $BranchDir, $SourceRevisionNumber)
        }
    }
}

sub new
{
    my ($Class, $RootURL, $BranchPath) = @_;

    my @BranchRevisions;
    my @MergeDownRevisions;

    my $Self = $Class->SUPER::new(
        BranchPath => $BranchPath,
        BranchRevisions => \@BranchRevisions,
        MergeDownRevisions => \@MergeDownRevisions
    );

    print STDERR "Gathering information about $BranchPath...\n";

    my $RevisionLog = $Self->{SVN}->ReadLog('--stop-on-copy',
        "-rHEAD\:1", $RootURL, $BranchPath);

    my %BranchStructure;
    my $CommonTarget = "/$BranchPath/";

    for my $Revision (@$RevisionLog)
    {
        push @BranchRevisions, $Revision;

        if ($Revision->{LogMessage} =~ m/^Created branch '(.+?)'.$/o &&
            $1 eq $BranchPath)
        {
            $Self->{BranchCreationRevision} = $Revision;

            $Self->ModelBranchStructure(\%BranchStructure,
                $Revision, $CommonTarget);

            last
        }
    }

    unless ($Self->{UpstreamPath} &&
        ($Self->{BranchSourceRevision} = $Self->{LastDownSyncRevisionNumber}))
    {
        die 'unable to determine branch source'
    }

    for my $Revision (reverse @BranchRevisions)
    {
        my $LogMessage = $Revision->{LogMessage};

        if ($LogMessage =~ m/^Modified branch '(.+?)'.$/o && $1 eq $BranchPath)
        {
            $Self->ModelBranchStructure(\%BranchStructure,
                $Revision, $CommonTarget)
        }
        elsif ($LogMessage =~
            m/^Merged changes up to r(\d+) from '.+' into '(.+)'.$/o &&
                $2 eq $BranchPath)
        {
            $Revision->{SourceRevisionNumber} =
                $Self->{LastDownSyncRevisionNumber} = $1;

            $Revision->{MergeDirection} = 'down';

            unshift @MergeDownRevisions, $Revision
        }
    }

    my @BranchPaths =
        sort @{NCBI::SVN::Branching::Util::FindPathsInTree(\%BranchStructure)};

    $Self->{BranchPaths} = \@BranchPaths;

    if ($Self->{ObsoleteBranchPaths})
    {
        ClearDeletedTree($Self->{ObsoleteBranchPaths}, \%BranchStructure);

        $Self->{ObsoleteBranchPaths} =
            NCBI::SVN::Branching::Util::FindPathsInTree(
                $Self->{ObsoleteBranchPaths})
    }

    $Self->{UpstreamPath} =~ s/^\/?(.+?)\/?$/$1/;

    return $Self
}

package NCBI::SVN::Branching::BranchAndUpstreamInfo;

use base qw(NCBI::SVN::Branching::BranchInfo);

sub new
{
    my ($Class, $RootURL, $BranchPath) = @_;

    my $Self = $Class->SUPER::new($RootURL, $BranchPath);

    my $UpstreamPath = $Self->{UpstreamPath};

    print STDERR "Gathering information about $UpstreamPath...\n";

    $Self->{LastUpSyncRevisionNumber} =
        $Self->{BranchCreationRevision}->{Number};

    my $UpstreamRevisions = $Self->{SVN}->ReadLog('--stop-on-copy',
        "-rHEAD\:$Self->{BranchRevisions}->[-1]->{Number}",
        $RootURL, map {"$UpstreamPath/$_"} @{$Self->{BranchPaths}});

    my @MergeUpRevisions;

    for my $Revision (reverse @$UpstreamRevisions)
    {
        if ($Revision->{LogMessage} =~
            m/^Merged changes up to r(\d+) from '(.+)' into '.+'.$/o &&
                $2 eq $BranchPath)
        {
            $Revision->{SourceRevisionNumber} =
                $Self->{LastUpSyncRevisionNumber} = $1;

            $Revision->{MergeDirection} = 'up';

            unshift @MergeUpRevisions, $Revision
        }
    }

    @$Self{qw(UpstreamRevisions MergeUpRevisions)} =
        ($UpstreamRevisions, \@MergeUpRevisions);

    $Self->{MergeRevisions} = [sort {$b->{Number} <=> $a->{Number}}
        @MergeUpRevisions, @{$Self->{MergeDownRevisions}}];

    return $Self
}

1
