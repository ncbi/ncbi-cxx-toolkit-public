# $Id$

package NCBI::SVN::Branching::Util;

use strict;
use warnings;

sub FindTreeNode
{
    my ($Tree, $Path) = @_;

    for my $Subdir (split('/', $Path))
    {
        $Tree = ($Tree->{$Subdir} ||= {})
    }

    return $Tree
}

sub SimplifyBranchPath
{
    my ($BranchPath) = @_;

    $BranchPath =~ s/^branches\///o;

    return $BranchPath
}

sub FindPathsInTreeRecursively
{
    my ($Paths, $Path, $Tree) = @_;

    while (my ($Name, $Subtree) = each %$Tree)
    {
        unless ($Name eq '/')
        {
            FindPathsInTreeRecursively($Paths, "$Path/$Name", $Subtree)
        }
        else
        {
            push @$Paths, $Path
        }
    }
}

sub FindPathsInTree
{
    my ($Tree) = @_;

    my @Paths;

    while (my ($Name, $Subtree) = each %$Tree)
    {
        FindPathsInTreeRecursively(\@Paths, $Name, $Subtree)
    }

    return \@Paths
}

sub MakeMergeLogMessage
{
    my ($SourcePath, $TargetPath,
        $FromRevisionNumber, $ToRevisionNumber, $MergeDescription) = @_;

    my $LogMessage = "changes from '$SourcePath' " .
        "(Revisions $FromRevisionNumber\:$ToRevisionNumber) " .
        "into '$TargetPath'.";

    $LogMessage .= "\n\n$MergeDescription" if $MergeDescription;

    return $LogMessage
}

sub ParseMergeLogMessage
{
    my ($LogMessage) = @_;

    if ($LogMessage =~
        m/changes from '(.+?)' \(Revisions (\d+):(\d+)\) into '(.+?)'\.\s*(.*)/so)
    {
        return ($1, $4, $2, $3, $5)
    }

    # Old format.
    if ($LogMessage =~
        m/changes up to r(\d+) from '(.+?)' into '(.+?)'\.\s*(.*)/so)
    {
        return ($2, $3, -1, $1, $4)
    }

    return ()
}

1
