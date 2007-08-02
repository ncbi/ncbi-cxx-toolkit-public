# $Id$

package NCBI::SVN::MultiSwitch;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use IPC::Open2;
use Carp qw(confess);

sub SwitchUsingMap
{
    my ($Self, $SwitchMap, $WorkingDir) = @_;

    confess 'SwitchMap parameter is missing' unless $SwitchMap;

    $WorkingDir ||= '.';
    $WorkingDir =~ s/^(?:\.\/)+//o;

    my @AlreadySwitchedDirs;

    {
        my $Stream = $Self->{SVN}->Run('stat',
            '--non-interactive', $WorkingDir);

        my $Line;

        while (defined($Line = $Stream->ReadLine()))
        {
            push @AlreadySwitchedDirs, $1 if $Line =~ m/^....S..(.*?)[\r\n]*$/os
        }
    }

    my $Info = $Self->{SVN}->ReadInfo('--non-interactive',
        $WorkingDir, @AlreadySwitchedDirs);

    my $WorkingDirInfo = $Info->{$WorkingDir};

    my $Repos = $WorkingDirInfo->{Root}
        or die "$Self->{MyName}\: unable to detect repository URL.\n";

    my $WorkingDirRepoPath = $WorkingDirInfo->{Path};

    my %AlreadySwitchedRepoPath;

    for my $SwitchedDir (@AlreadySwitchedDirs)
    {
        my $RepoPath = $Info->{$SwitchedDir}->{Path};

        substr($SwitchedDir, 0, length($WorkingDir) + 1, '')
            eq "$WorkingDir/" or die if $WorkingDir ne '.';

        $AlreadySwitchedRepoPath{$SwitchedDir} = $RepoPath
    }

    for (@$SwitchMap)
    {
        my ($DirToSwitch, $RepoPathToSwitchTo) = @$_;

        my $AlreadySwitchedTo = delete $AlreadySwitchedRepoPath{$DirToSwitch};

        if ($AlreadySwitchedTo && $AlreadySwitchedTo eq $RepoPathToSwitchTo)
        {
            print "Skipping '$DirToSwitch': already switched.\n"
        }
        else
        {
            print "Switching '$DirToSwitch' to '$RepoPathToSwitchTo'...\n";

            $Self->{SVN}->RunSubversion('switch',
                "$Repos/$RepoPathToSwitchTo", "$WorkingDir/$DirToSwitch")
        }
    }

    for my $Dir (keys %AlreadySwitchedRepoPath)
    {
        print "Unswitching '$Dir'...\n";

        $Self->{SVN}->RunSubversion('switch',
            "$Repos/$WorkingDirRepoPath/$Dir", "$WorkingDir/$Dir")
    }
}

1
