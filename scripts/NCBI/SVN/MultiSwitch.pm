# $Id$

package NCBI::SVN::MultiSwitch;

use strict;
use warnings;
use Carp qw(confess);

use base qw(NCBI::SVN::Base);

sub SwitchUsingMap
{
    my ($Self, $MapFileName, $Repos, $WorkingDir) = @_;

    my @SwitchPlan;

    open FILE, '<', $MapFileName or die "$Self->{MyName}: $MapFileName\: $!\n";

    while (<FILE>)
    {
        chomp;

        my ($FromDir, $ToDir) = split;

        map {s/[;<>|&\\]+|\/{2,}/\//gos} $FromDir, $ToDir;
        m/(?:^|\/)\.\.(?:\/|$)/o &&
            die "$Self->{MyName}: $MapFileName\:$.: path '$_' contains '..'\n"
            for $FromDir, $ToDir;

        $FromDir = "$WorkingDir/$FromDir" if $WorkingDir;

        push @SwitchPlan, [$FromDir, $ToDir]
    }

    close FILE;

    for (@SwitchPlan)
    {
        my ($FromDir, $ToDir) = @$_;

        print "Switching '$FromDir' to '$ToDir'...\n";

        system $Self->{SvnPath}, 'switch', "$Repos/$ToDir", $FromDir
    }
}

1
