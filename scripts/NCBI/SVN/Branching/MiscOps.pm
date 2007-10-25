# $Id$

package NCBI::SVN::Branching::MiscOps;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::Util;
use NCBI::SVN::Branching::BranchInfo;

sub List
{
    my ($Self, $RootURL) = @_;

    print "Managed branches in $RootURL\:\n";

    print NCBI::SVN::Branching::Util::SimplifyBranchPath($_) . "\n"
        for $Self->{SVN}->ReadFileLines($RootURL . '/branches/branch_list')
}

sub PrintMergeRevisionHistory
{
    my ($Self, $Direction, $SourcePath, $TargetPath, $Revisions) = @_;

    sub Times
    {
        return @{$_[0]} ? @{$_[0]} > 1 ? @{$_[0]} . ' times' : 'once' : 'never'
    }

    print "Merged [$Direction] from '$SourcePath' into '$TargetPath': " .
        Times($Revisions) . "\n";

    for my $Revision (@$Revisions)
    {
        my $Line = "  r$Revision->{Number} (from " .
            "r$Revision->{SourceRevisionNumber})";

        if (my $Description = $Revision->{MergeDescription})
        {
            $Line .= "\t- ";
            $Description =~ s/\s+$//so;
            $Description =~ s/^\s+//so;
            $Description =~ s/\s+/ /gso;
            $Description = "$1..." if $Description =~ m/^(.{37}).{4}/;
            $Line .= $Description
        }

        print $Line . "\n"
    }
}

sub Info
{
    my ($Self, $RootURL, $BranchPath) = @_;

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);
    my $UpstreamInfo = NCBI::SVN::Branching::UpstreamInfo->new($BranchInfo);

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    print "Branch path: $BranchPath\n" .
        "Created: in r$BranchInfo->{BranchCreationRevision}->{Number}" .
        " from $UpstreamPath\@$BranchInfo->{BranchSourceRevision}\n";

    print "Branch structure:\n";
    for my $Path (@{$BranchInfo->{BranchPaths}})
    {
        print "  $Path\n"
    }

    $Self->PrintMergeRevisionHistory('up', $BranchPath, $UpstreamPath,
        $UpstreamInfo->{MergeUpRevisions});

    $Self->PrintMergeRevisionHistory('down', $UpstreamPath, $BranchPath,
        $BranchInfo->{MergeDownRevisions})
}

1
