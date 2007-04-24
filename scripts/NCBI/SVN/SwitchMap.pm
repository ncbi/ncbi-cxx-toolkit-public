# $Id$

package NCBI::SVN::SwitchMap;

use strict;
use warnings;
use Carp qw(confess);

use base qw(NCBI::SVN::Base);

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    if (my $MapFileName = $Self->{MapFileName})
    {
        $Self->ReadFromFile($MapFileName)
    }
    elsif (my $MapFileLines = $Self->{MapFileLines})
    {
        $Self->SetFromLines($MapFileLines)
    }

    return $Self
}

sub ReadFromFile
{
    my ($Self, $MapFileName) = @_;

    my @SwitchPlan;

    open FILE, '<', $MapFileName or die "$Self->{MyName}: $MapFileName\: $!\n";

    while (<FILE>)
    {
        s/\s+$//os;

        push @SwitchPlan, [split]
    }

    close FILE;

    $Self->SetSwitchPlan(\@SwitchPlan)
}

sub SetFromLines
{
    my ($Self, $MapFileLines) = @_;

    my @SwitchPlan;

    for (@$MapFileLines)
    {
        s/\s+$//os;

        push @SwitchPlan, [split]
    }

    $Self->SetSwitchPlan(\@SwitchPlan)
}

sub SetSwitchPlan
{
    my ($Self, $SwitchPlan) = @_;

    # Verify the switch plan.
    my %PathReg;

    for (@$SwitchPlan)
    {
        for (@$_)
        {
            s/[;<>|&\\]+|\/{2,}/\//gos;

            m/(?:^|\/)\.\.(?:\/|$)/o &&
                die "$Self->{MyName}: map file contains '..'\n";

            $PathReg{$_} &&
                die "$Self->{MyName}: directory mapping conflict\n";

            $PathReg{$_} = 1
        }
    }

    $Self->{SwitchPlan} = $SwitchPlan
}

sub GetSwitchPlan
{
    my ($Self) = @_;

    return $Self->{SwitchPlan}
}

sub DiffSwitchPlan
{
    my ($Self, $RightSide, $InvertDirection) = @_;

    my @Diff;

    my ($FromIndex, $ToIndex) = $InvertDirection ? (1, 0) : (0, 1);

    my %Map = map {$_->[$ToIndex] => $_->[$FromIndex]} @{$Self->{SwitchPlan}};

    for my $NewSwitch (@{$RightSide->{SwitchPlan}})
    {
        my ($NewFrom, $NewTo) = @$NewSwitch[$FromIndex, $ToIndex];

        my $OldFrom = delete $Map{$NewTo};

        unless ($OldFrom && $OldFrom eq $NewSwitch->[$FromIndex])
        {
            push @Diff, [$NewTo, $NewFrom, $OldFrom]
        }
    }

    while (my ($OldTo, $OldFrom) = each %Map)
    {
        unshift @Diff, [$OldTo, undef, $OldFrom]
    }

    return @Diff
}

1
