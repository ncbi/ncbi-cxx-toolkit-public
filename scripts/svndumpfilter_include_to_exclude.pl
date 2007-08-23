#!/usr/bin/perl -w
# $Id$

use strict;
use warnings;

my ($ScriptDir, $ScriptName);

BEGIN
{
    ($ScriptDir, $ScriptName) = $0 =~ m/^(?:(.+)[\\\/])?(.+)$/;
    $ScriptDir ||= '.'
}

use lib $ScriptDir;

use NCBI::SVN::Wrapper;

my ($Repository, @Paths) = @ARGV;

die "Usage: $0 <repository_url> <include_path> ...\n" unless @Paths;

my %IncludeTree;

Path:
for my $Path (@Paths)
{
    my $Branch = \%IncludeTree;

    while ($Path =~ m/(.*?)\/(.*)/)
    {
        $Path = $2;

        next unless $1;

        my $Dir = $1;

        if (exists($Branch->{$Dir}))
        {
            next Path unless ref($Branch = $Branch->{$Dir})
        }
        else
        {
            $Branch = $Branch->{$Dir} = {}
        }
    }

    $Branch->{$Path} = undef if $Path
}

my $SVN = NCBI::SVN::Wrapper->new();

sub IncludeToExclude
{
    my ($Dir, $IncludeTree) = @_;

    for my $Entry ($SVN->ReadSubversionLines('list', "$Repository/$Dir"))
    {
        $Entry =~ s/\/$//so;

        unless (exists $IncludeTree->{$Entry})
        {
            print "$Dir/$Entry\n"
        }
        elsif (my $TreeBranch = $IncludeTree->{$Entry})
        {
            IncludeToExclude("$Dir/$Entry", $TreeBranch)
        }
    }
}

IncludeToExclude('', \%IncludeTree)
