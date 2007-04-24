# $Id$

package NCBI::SVN::Wrapper;

use base qw(NCBI::SVN::Base);

use strict;
use warnings;
use Carp qw(confess);

use IPC::Open3;

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    if ($Self->{Repos})
    {
        $Self->{Repos} =~ s/\/+$//;
    }
    else
    {
        $Self->{Repos} = 'file:///home/kazimird/repos'
    }

    return $Self
}

sub GetRepos
{
    my ($Self) = @_;

    return $Self->{Repos}
}

sub ReadFile
{
    my ($Self, $FileName) = @_;

    return $Self->ReadSubversionStream('cat', "$Self->{Repos}/$FileName")
}

sub ReadFileLines
{
    my ($Self, $FileName) = @_;

    return $Self->ReadSubversionLines('cat', "$Self->{Repos}/$FileName")
}

sub ReadSubversionStream
{
    my ($Self, @CommandAndParams) = @_;

    my ($ReadFH, $WriteFH);

    my $PID = open3($WriteFH, $ReadFH, undef, $Self->GetSvnPath(),
        '--non-interactive', @CommandAndParams);

    close $WriteFH;

    my $Buffer = '';

    while (read $ReadFH, $Buffer, 16, length($Buffer))
    {
    }

    close $ReadFH;

    waitpid $PID, 0;

    die $Buffer if $?;

    return $Buffer
}

sub ReadSubversionLines
{
    my ($Self, @CommandAndParams) = @_;

    my ($ReadFH, $WriteFH);

    my $PID = open3($WriteFH, $ReadFH, undef, $Self->GetSvnPath(),
        '--non-interactive', @CommandAndParams);

    close $WriteFH;

    my @Lines;

    while (<$ReadFH>)
    {
        chomp;
        push @Lines, $_
    }

    close $ReadFH;

    waitpid $PID, 0;

    die join("\n", @Lines) . "\n" if $?;

    return @Lines
}

1
