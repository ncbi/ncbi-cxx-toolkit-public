# $Id$

package NCBI::SVN::MultiSwitch;

use strict;
use warnings;
use Carp qw(confess);

use IPC::Open2;

use base qw(NCBI::SVN::Base);

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    my ($MapFileName, $WorkingDir) = @$Self{qw(MapFileName WorkingDir)};

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

        $FromDir = "$WorkingDir/$FromDir" if $WorkingDir;

        push @SwitchPlan, [$FromDir, $ToDir]
    }

    close FILE;

    $Self->{SwitchPlan} = \@SwitchPlan;

    return $Self
}

sub SwitchUsingMap
{
    my ($Self, $Repos) = @_;

    my %AlreadySwitched;

    my ($ReadHandle, $WriteHandle);

    my $PID = open2($ReadHandle, $WriteHandle,
        'svn', 'stat', '--non-interactive', $Self->{WorkingDir} || '.');

    close($WriteHandle);

    while (<$ReadHandle>)
    {
        $AlreadySwitched{$1} = undef if m/^....S..(.*?)[\r\n]*$/os
    }

    close($ReadHandle);

    waitpid $PID, 0;

    if (%AlreadySwitched)
    {
        $PID = open2($ReadHandle, $WriteHandle,
            'svn', 'info', '--non-interactive', keys %AlreadySwitched);

        close($WriteHandle);

        my $Path;

        while (<$ReadHandle>)
        {
            if (m/^Path: (.*?)[\r\n]*$/os)
            {
                $Path = $1
            }
            elsif (m/^URL: (.*?)[\r\n]*$/os)
            {
                $AlreadySwitched{$Path} = $1
            }
        }

        close($ReadHandle);

        waitpid $PID, 0
    }

    for (@{$Self->{SwitchPlan}})
    {
        my ($FromDir, $ToDir) = @$_;

        my $URL = "$Repos/$ToDir";

        if ($AlreadySwitched{$FromDir} && $AlreadySwitched{$FromDir} eq $URL)
        {
            print "Skipping '$FromDir': already switched.\n"
        }
        else
        {
            print "Switching '$FromDir' to '$ToDir'...\n";

            system $Self->{SvnPath}, 'switch', $URL, $FromDir
        }

        delete $AlreadySwitched{$FromDir}
    }

    for my $Dir (keys %AlreadySwitched)
    {
        print "Unswitching '$Dir'...\n";

        system $Self->{SvnPath}, 'switch', "$Repos/$Dir", $Dir
    }
}

1
