# $Id$

use strict;
use warnings;
use Carp qw(confess);

package NCBI::SVN::Wrapper::Stream;

use base qw(NCBI::SVN::Base);

use IPC::Open3;
use File::Temp qw/tempfile/;

sub Run
{
    my ($Self, @Args) = @_;

    my $WriteFH;

    $Self->{PID} = open3($WriteFH, $Self->{ReadFH},
        $Self->{ErrorFH} = tempfile(),
        $Self->GetSvnPath(), '--non-interactive', @Args);

    close $WriteFH
}

sub ReadLine
{
    my ($Self) = @_;

    my $Line = readline $Self->{ReadFH};

    chomp $Line if $Line;

    return $Line
}

sub Close
{
    my ($Self) = @_;

    return unless exists $Self->{PID};

    local $/ = undef;
    my $ErrorText = readline $Self->{ErrorFH};

    close($Self->{ReadFH});
    close($Self->{ErrorFH});

    waitpid(delete $Self->{PID}, 0);

    die $ErrorText if $?
}

package NCBI::SVN::Wrapper;

use base qw(NCBI::SVN::Base);

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    if ($Self->{Repos})
    {
        $Self->{Repos} =~ s/\/+$//;
    }
    elsif (-d '.svn')
    {
        $Self->{Repos} = $Self->ReadInfo('.')->{'.'}->{Root}
    }
    else
    {
        $Self->{Repos} = 'https://svn.ncbi.nlm.nih.gov/repos/toolkit'
    }

    return $Self
}

sub SetRepository
{
    my ($Self, $Repository) = @_;

    $Self->{Repos} = $Repository
}

sub GetRepository
{
    my ($Self) = @_;

    return $Self->{Repos}
}

sub Run
{
    my ($Self, @Args) = @_;

    my $Stream = NCBI::SVN::Wrapper::Stream->new();

    $Stream->Run(@Args);

    return $Stream
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

sub ReadInfo
{
    my ($Self, @Dirs) = @_;

    my $Stream = $Self->Run('info', @Dirs);

    my %Info;
    my $Path;
    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        if ($Line =~ m/^Path: (.*?)$/os)
        {
            $Path = $1
        }
        elsif ($Line =~ m/^URL: (.*?)$/os)
        {
            $Info{$Path}->{Path} = $1
        }
        elsif ($Line =~ m/^Repository Root: (.*?)$/os)
        {
            $Info{$Path}->{Root} = $1
        }
    }

    for my $DirInfo (values %Info)
    {
        my ($Root, $Path) = @$DirInfo{qw(Root Path)};


        substr($Path, 0, length($Root), '') eq $Root or die;

        $DirInfo->{Path} = length($Path) > 0 ?
            substr($Path, 0, 1, '') eq '/' ? $Path : die : '.'
    }

    $Stream->Close();

    return \%Info
}

sub ReadLog
{
    my ($Self, @LogParams) = @_;

    my $Stream = $Self->Run('log', '--verbose', @LogParams);

    my $Line;
    my $State = 'initial';
    my $ErrorMessage = 'svn log parsing error';

    my @Revisions;
    my $CurrentRevision;
    my $NumberOfLogLines;

    while (defined($Line = $Stream->ReadLine()))
    {
        $Line =~ s/[\r\n]+$//;

        if ($State eq 'changed_path')
        {
            if ($Line)
            {
                $Line =~ m/^   ([AMD]) (.+?)(?: \(from (.+):(\d+)\))?$/o or
                    goto ParsingError;

                push @{$CurrentRevision->{ChangedPaths}}, [$1, $2, $3, $4]
            }
            else
            {
                $State = 'log_message';
            }
        }
        elsif ($State eq 'log_message')
        {
            $State = 'initial' if --$NumberOfLogLines == 0;
            $CurrentRevision->{LogMessage} .= $Line . "\n"
        }
        elsif ($State eq 'revision_header')
        {
            my %NewRev = (LogMessage => '', ChangedPaths => []);

            push @Revisions, ($CurrentRevision = \%NewRev);

            (@NewRev{qw(Number Author)}, $NumberOfLogLines) = $Line =~
                m/^r(\d+) \| (.+?) \| .+? \| (\d) lines?$/o or
                    goto ParsingError;

            $State = $NumberOfLogLines > 0 ? 'changed_path_header' : 'initial'
        }
        elsif ($State eq 'changed_path_header')
        {
            goto ParsingError if $Line ne 'Changed paths:';

            $State = 'changed_path'
        }
        elsif ($State eq 'initial')
        {
            goto ParsingError unless $Line =~ m/^-{70}/o;

            $State = 'revision_header'
        }
    }

    $Stream->Close();

    chomp $_->{LogMessage} for @Revisions;

    return \@Revisions;

ParsingError:
    local $/ = undef;
    $Stream->ReadLine();
    $Stream->Close();
    die 'svn log parsing error'
}

sub GetLatestRevision
{
    my ($Self) = @_;

    my $Stream = $Self->Run('info', $Self->GetRepository());

    my $Line;
    my $Revision;

    while (defined($Line = $Stream->ReadLine()))
    {
        if ($Line =~ m/^Revision: (.*)$/os)
        {
            $Revision = $1;
            last
        }
    }

    while (defined($Stream->ReadLine()))
    {
    }

    $Stream->Close();

    return $Revision
}

sub ReadSubversionStream
{
    my ($Self, @CommandAndParams) = @_;

    my $Stream = $Self->Run(@CommandAndParams);

    local($/) = undef;

    my $Contents = $Stream->ReadLine();

    $Stream->Close();

    return $Contents
}

sub ReadSubversionLines
{
    my ($Self, @CommandAndParams) = @_;

    my $Stream = $Self->Run(@CommandAndParams);

    my @Lines;
    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        push @Lines, $Line
    }

    $Stream->Close();

    return @Lines
}

1
