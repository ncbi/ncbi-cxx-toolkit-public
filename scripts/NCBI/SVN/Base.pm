# $Id$

package NCBI::SVN::Base;

use strict;

use NCBI::SVN::Wrapper;

our $SvnWrapperCache;

sub new
{
    my ($Class, @Params) = @_;

    my %Self = @Params;

    $Self{MyName} ||= $Class;
    $Self{SVN} ||= ($SvnWrapperCache ||= NCBI::SVN::Wrapper->new(@Params));

    return bless \%Self, $Class
}

sub SetSvn
{
    my ($Self, $SVN) = @_;

    $Self->{SVN} = $SVN
}

sub GetSvn
{
    my ($Self) = @_;

    return $Self->{SVN}
}

1
