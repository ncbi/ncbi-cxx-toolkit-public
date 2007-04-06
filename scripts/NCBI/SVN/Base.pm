# $Id$

package NCBI::SVN::Base;

use strict;
use warnings;
use Carp qw(confess);

use File::Spec;

our $SvnPathCache;

sub FindSubversion
{
    for my $Path (File::Spec->path())
    {
        for my $Program qw(svn svn.bat svn.exe)
        {
            my $Pathname = File::Spec->catfile($Path, $Program);

            return $Pathname if -x $Pathname
        }
    }

    confess 'Unable to find "svn" in PATH'
}

sub new
{
    my ($Class, @Params) = @_;

    my %Param = @Params;

    $Param{MyName} ||= $Class;

    return bless \%Param, $Class
}

sub GetSvnPath
{
    my ($Self) = @_;

    return $Self->{SvnPath} ||= ($SvnPathCache ||= FindSubversion())
}

sub RunSubversion
{
    my ($Self, @Params) = @_;

    return system $Self->GetSvnPath(), @Params
}

sub CallSubversion
{
    my ($Self, @Params) = @_;

    local $" = '" "';

    my $SvnPath = $Self->GetSvnPath();
    my @Output = `"$SvnPath" "@Params"`;
    chomp @Output;

    return @Output
}

1
