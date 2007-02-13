# $Id$

package NCBI::SVN::Update;

use base qw(NCBI::SVN::Base);

use strict;
use warnings;
use Carp qw(confess);

use File::Find;

sub new
{
    my $Self = NCBI::SVN::Base::new(@_);

    $Self->{BufferSize} = 4096;

    return $Self
}

sub CreateEmpty
{
    my (undef, $Pathname) = @_;

    open FILE, '>', $Pathname or confess "$Pathname\: $!";
    close FILE
}

sub SwitchToRecursive
{
    my ($Self, $Dir) = @_;

    my $RFile = "$Dir/.svn/ncbi-R";

    unless (-f $RFile)
    {
        unlink "$Dir/.svn/ncbi-N";

        print "L $Dir\n";

        for my $SubDir ($Self->CallSubversion('list', $Dir))
        {
            if ($SubDir =~ m/^(.+)\/$/os)
            {
                $SubDir = "$Dir/$1";

                if (-d $SubDir)
                {
                    $Self->SwitchToRecursive($SubDir)
                }
                else
                {
                    push @{$Self->{RecursiveUpdates}}, $SubDir
                }
            }
        }

        $Self->CreateEmpty($RFile)
    }
}

sub NewTreeTraversal
{
    my ($Self, $Branch, $Path) = @_;

    while (my ($Dir, $Contents) = each %$Branch)
    {
        $Dir = "$Path/$Dir" if $Path;

        if (ref $Contents)
        {
            push @{$Self->{NonRecursiveUpdates}}, $Dir;

            $Self->NewTreeTraversal($Contents, $Dir)
        }
        else
        {
            push @{$Self->{RecursiveUpdates}}, $Dir;

            $Self->SwitchToRecursive($Dir) if -d $Dir
        }
    }
}

sub ReadNFile
{
    my ($Self, $FileName) = @_;

    my @DirNames;

    if (open FILE, '<', $FileName)
    {
        while (<FILE>)
        {
            chomp;
            push @DirNames, $_
        }

        close FILE
    }

    return @DirNames
}

sub SwitchToNonRecursive
{
    my ($Self, $Branch, $Path) = @_;

    unlink "$Path/.svn/ncbi-R";

    my $NFile = "$Path/.svn/ncbi-N";

    my %Exists = map {$_ => 1} $Self->ReadNFile($NFile);

    open FILE, '>>', $NFile or die "$NFile\: $!";

    while (my ($Dir, $Contents) = each %$Branch)
    {
        print FILE "$Dir\n" unless $Exists{$Dir}
    }

    close FILE;

    while (my ($Dir, $Contents) = each %$Branch)
    {
        if (ref $Contents)
        {
            $Dir = "$Path/$Dir";

            push @{$Self->{NonRecursiveUpdates}}, $Dir;

            $Self->SwitchToNonRecursive($Contents, $Dir)
        }
    }
}

sub UpdateDirList
{
    my ($Self, @Paths) = @_;

    confess 'List of directories to update is empty' unless @Paths;
    confess 'Not in a working copy directory' unless -d '.svn';

    delete @$Self{qw(NonRecursiveUpdates RecursiveUpdates)};

    my %NewTree;

Path:
    for my $Path (@Paths)
    {
        $Path =~ m/(?:^|\/)\.\.(?:\/|$)/o && die "Path '$Path' contains '..'\n";

        $Path =~ s/\/\.(?=\/|$)//go;

        $Path = './' . $Path unless $Path =~ m/^\.(?:\/|$)/o;

        my $Branch = \%NewTree;

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

    $Self->NewTreeTraversal(\%NewTree);

    if ($Self->{NonRecursiveUpdates})
    {
        print "Performing non-recursive updates:\n";

        system $Self->{SvnPath}, 'update', '-N', @{$Self->{NonRecursiveUpdates}}
    }

    $Self->SwitchToNonRecursive($NewTree{'.'}, '.') if $NewTree{'.'};

    if ($Self->{RecursiveUpdates})
    {
        print "Performing recursive updates:\n";

        system $Self->{SvnPath}, 'update', @{$Self->{RecursiveUpdates}};

        find(sub
        {
            if (m/^\.svn$/)
            {
                unlink '.svn/ncbi-N';
                $Self->CreateEmpty('.svn/ncbi-R') unless -f '.svn/ncbi-R'
            }
        }, @{$Self->{RecursiveUpdates}})
    }
}

sub CollectUpdates
{
    my ($Self, $Dir) = @_;

    confess "Directory '$Dir' is not a working copy" unless -d "$Dir/.svn";

    if (-f "$Dir/.svn/ncbi-R")
    {
        push @{$Self->{RecursiveUpdates}}, $Dir
    }
    elsif (-f "$Dir/.svn/ncbi-N")
    {
        push @{$Self->{NonRecursiveUpdates}}, $Dir;

        for my $SubDir ($Self->ReadNFile("$Dir/.svn/ncbi-N"))
        {
            $Self->CollectUpdates("$Dir/$SubDir")
        }
    }
    else
    {
        confess "'$Dir' must be explicitly updated by $Self->{MyName} first"
    }
}

sub UpdateCWD
{
    my ($Self) = @_;

    delete @$Self{qw(NonRecursiveUpdates RecursiveUpdates)};

    $Self->CollectUpdates('.');

    local $, = ' ';

    if ($Self->{NonRecursiveUpdates})
    {
        print "Performing non-recursive updates:\n";

        system $Self->{SvnPath}, 'update', '-N', @{$Self->{NonRecursiveUpdates}}
    }

    if ($Self->{RecursiveUpdates})
    {
        print "Performing recursive updates:\n";

        system $Self->{SvnPath}, 'update', @{$Self->{RecursiveUpdates}}
    }
}

1
