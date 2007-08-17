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

1
