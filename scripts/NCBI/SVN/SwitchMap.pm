# $Id$

package NCBI::SVN::SwitchMap;

use strict;
use warnings;
use Carp qw(confess);

use base qw(NCBI::SVN::Base);

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    my $MapFileName = $Self->{MapFileName};

    confess 'MapFileName parameter is missing' unless $MapFileName;

    my @SwitchPlan;

    open FILE, '<', $MapFileName or die "$Self->{MyName}: $MapFileName\: $!\n";

    while (<FILE>)
    {
        s/[\r\n]+$//os;

        my ($FromDir, $ToDir) = split;

        map {s/[;<>|&\\]+|\/{2,}/\//gos} $FromDir, $ToDir;
        m/(?:^|\/)\.\.(?:\/|$)/o &&
            die "$Self->{MyName}: $MapFileName\:$.: path '$_' contains '..'\n"
            for $FromDir, $ToDir;

        push @SwitchPlan, [$FromDir, $ToDir]
    }

    close FILE;

    $Self->{SwitchPlan} = \@SwitchPlan;

    return $Self
}

sub GetSwitchPlan
{
    my ($Self) = @_;

    return $Self->{SwitchPlan}
}

1
