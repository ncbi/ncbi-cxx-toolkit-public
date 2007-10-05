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

1
