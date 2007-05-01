#!/usr/bin/perl -w
# $Id$

use strict;

use IPC::Open2;

my @FileNames = @ARGV;

die "Usage: $0 FILE ...\n" unless @FileNames;

s/^\.\/// for @FileNames;

my @MaskProps;

my $SvnConfig = '/etc/subversion/config';

open CONFIG, '<', $SvnConfig or die "$0: unable to open $SvnConfig\: $!";

while (<CONFIG>)
{
    last if m/\[auto-props\]/o
}

while (<CONFIG>)
{
    chomp;
    next if m/^\s*(#|$)/o;

    my ($Mask, $Props) = split(m/\s*=\s*/, $_, 2);

    $Mask = quotemeta($Mask);
    $Mask =~ s/\\\*/.*/;

    push @MaskProps,
        [qr/^$Mask$/, {map {split(m/\s*=\s*/, $_, 2)} split(';', $Props)}]
}

close CONFIG;

my ($ReadFH, $WriteFH);

my $ChildPID = open2($ReadFH, $WriteFH, 'svn', '--non-interactive',
    'proplist', '--verbose', @FileNames);

close $WriteFH;

my %ExistingProps;

my $Line = readline $ReadFH;
my $FileName;

if (defined($Line))
{
    chomp($Line);
    die unless $Line =~ m/^Properties on '(.*)':$/;

    $FileName = $1;

NextFileProps:
    while (defined($Line = readline($ReadFH)))
    {
        chomp $Line;

        if ($Line =~ m/^Properties on '(.*)':$/)
        {
            $FileName = $1;
            goto NextFileProps
        }

        if ($Line =~ m/^\s+([^\s]+)\s+:\s+(.*)$/o)
        {
            $ExistingProps{$FileName}->{$1} = $2
        }
    }
}

close $ReadFH;

waitpid $ChildPID, 0;

my %PropsToSet;

NextFile:
for my $FileName (sort @FileNames)
{
    my $FileProps = $ExistingProps{$FileName};

    my ($BaseName) = $FileName =~ m/([^\/]+)$/;

    for my $MaskProps (@MaskProps)
    {
        if ($BaseName =~ $MaskProps->[0])
        {
            while (my ($PropName, $PropValue) = each %{$MaskProps->[1]})
            {
                unless ($FileProps->{$PropName} &&
                    $FileProps->{$PropName} eq $PropValue)
                {
                    push @{$PropsToSet{$PropName}->{$PropValue}}, $FileName
                }
            }
            next NextFile
        }
    }

    if ($FileProps->{'svn:mime-type'})
    {
        push @{$PropsToSet{'svn:eol-style'}->{'native'}}, $FileName
            if $FileProps->{'svn:mime-type'} =~ m/^text\//o &&
                !$FileProps->{'svn:eol-style'};

        next NextFile
    }

    my $IsText;

    $ChildPID = open2($ReadFH, $WriteFH, 'file', $FileName);

    close $WriteFH;

    while (<$ReadFH>)
    {
        $IsText = 1 if m/text/io
    }

    close $ReadFH;

    waitpid $ChildPID, 0;

    if ($IsText)
    {
        push @{$PropsToSet{'svn:mime-type'}->{'text/plain'}}, $FileName;
        push @{$PropsToSet{'svn:eol-style'}->{'native'}}, $FileName
    }
    else
    {
        push @{$PropsToSet{'svn:mime-type'}->{'application/octet-stream'}},
            $FileName
    }
}

while (my ($PropName, $PropVals) = each %PropsToSet)
{
    while (my ($PropValue, $Files) = each %$PropVals)
    {
        system('svn', '--non-interactive', 'propset',
            $PropName, $PropValue, @$Files)
    }
}
