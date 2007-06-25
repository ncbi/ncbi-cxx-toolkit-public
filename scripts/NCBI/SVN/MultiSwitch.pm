# $Id$

package NCBI::SVN::MultiSwitch;

use strict;
use warnings;
use Carp qw(confess);

use IPC::Open2;

use base qw(NCBI::SVN::Base);

sub SwitchUsingMap
{
    my ($Self, $SwitchMap, $WorkingDir) = @_;

    confess 'SwitchMap parameter is missing' unless $SwitchMap;

    $WorkingDir ||= '.';
    $WorkingDir =~ s/^(?:\.\/)+//o;

    my $Repos;
    my @AlreadySwitched;

    my ($ReadHandle, $WriteHandle);

    my $PID = open2($ReadHandle, $WriteHandle,
        'svn', 'stat', '--non-interactive', $WorkingDir);

    close($WriteHandle);

    while (<$ReadHandle>)
    {
        push @AlreadySwitched, $1 if m/^....S..(.*?)[\r\n]*$/os
    }

    close($ReadHandle);

    waitpid $PID, 0;

    $PID = open2($ReadHandle, $WriteHandle, 'svn', 'info',
        '--non-interactive', $WorkingDir, @AlreadySwitched);

    close($WriteHandle);

    my $WorkingDirURL;
    my %AlreadySwitchedURL;
    my $Path;

    while (<$ReadHandle>)
    {
        if (m/^Path: (.*?)[\r\n]*$/os)
        {
            $Path = $1
        }
        elsif (m/^URL: (.*?)[\r\n]*$/os)
        {
            if ($Path eq $WorkingDir)
            {
                $WorkingDirURL = $1
            }
            else
            {
                substr($Path, 0, length($WorkingDir) + 1, '')
                    eq "$WorkingDir/" or die if $WorkingDir ne '.';

                $AlreadySwitchedURL{$Path} = $1
            }
        }
        elsif (!$Repos && m/^Repository Root: (.*?)[\r\n]*$/os)
        {
            $Repos = $1
        }
    }

    close($ReadHandle);

    waitpid $PID, 0;

    die "$Self->{MyName}\: unable to detect repository URL.\n" unless $Repos;

    for (@$SwitchMap)
    {
        my ($FromDir, $ToDir) = @$_;

        my $AlreadySwitchedURL = delete $AlreadySwitchedURL{$FromDir};
        my $URL = "$Repos/$ToDir";

        if ($AlreadySwitchedURL && $AlreadySwitchedURL eq $URL)
        {
            print "Skipping '$FromDir': already switched.\n"
        }
        else
        {
            print "Switching '$FromDir' to '$ToDir'...\n";

            $Self->RunSubversion('switch', $URL, "$WorkingDir/$FromDir")
        }
    }

    for my $Dir (keys %AlreadySwitchedURL)
    {
        print "Unswitching '$Dir'...\n";

        $Self->RunSubversion('switch',
            "$WorkingDirURL/$Dir", "$WorkingDir/$Dir")
    }
}

1
