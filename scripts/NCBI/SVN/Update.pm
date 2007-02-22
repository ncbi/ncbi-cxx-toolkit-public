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

sub SwitchToRecursive
{
    my ($Self, $Dir) = @_;

    my $RFile = "$Dir/.svn/ncbi-R";

    unless (-f $RFile)
    {
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
    }
}

sub IncludeNonExistingDir
{
    my ($Self, $Path, $Branch) = @_;

    if (ref $Branch)
    {
        push @{$Self->{NonRecursiveUpdates}}, $Path;

        while (my ($SubDir, $SubBranch) = each %$Branch)
        {
            $Self->IncludeNonExistingDir("$Path/$SubDir", $SubBranch)
        }
    }
    else
    {
        push @{$Self->{RecursiveUpdates}}, $Path
    }
}

sub IncludeExistingDir
{
    my ($Self, $Path, $ParentBranch, $Dir) = @_;

    my $Branch = $ParentBranch->{$Dir};

    unless (-d $Path)
    {
        $Self->IncludeNonExistingDir($Path, $Branch)
    }
    else
    {
        die "$Self->{MyName}: directory '$Path' is not under source control.\n"
            unless -d "$Path/.svn";

        unless (ref $Branch)
        {
            $Self->SwitchToRecursive($Path);

            push @{$Self->{RecursiveUpdates}}, $Path
        }
        elsif (-f "$Path/.svn/ncbi-R")
        {
            $ParentBranch->{$Dir} = undef;

            push @{$Self->{RecursiveUpdates}}, $Path
        }
        else
        {
            push @{$Self->{NonRecursiveUpdates}}, $Path;

            for my $SubDir ($Self->ReadNFile("$Path/.svn/ncbi-N"))
            {
                $Self->CollectUpdates("$Path/$SubDir")
                    unless exists $Branch->{$SubDir}
            }

            for my $SubDir (keys %$Branch)
            {
                $Self->IncludeExistingDir("$Path/$SubDir", $Branch, $SubDir)
            }
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

sub CreateNFiles
{
    my ($Self, $Branch, $Path) = @_;

    return unless ref $Branch;

    unlink "$Path/.svn/ncbi-R";

    my $NFile = "$Path/.svn/ncbi-N";

    my @OldDirs = grep {!exists $Branch->{$_} && -d "$Path/$_"}
        $Self->ReadNFile($NFile);

    my @NewDirs = grep {-d "$Path/$_"} keys %$Branch;

    open FILE, '>', $NFile or confess "$NFile\: $!";

    for my $SubDir (@OldDirs, @NewDirs)
    {
        print FILE "$SubDir\n"
    }

    close FILE;

    for my $SubDir (@NewDirs)
    {
        $Self->CreateNFiles($Branch->{$SubDir}, "$Path/$SubDir")
    }
}

sub PerformUpdates
{
    my ($Self) = @_;

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
        $Path =~ m/(?:^|\/)\.\.(?:\/|$)/o &&
            die "$Self->{MyName}: path '$Path' contains '..'\n";

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

    $Self->IncludeExistingDir('.', \%NewTree, '.');

    $Self->PerformUpdates();

    $Self->CreateNFiles($NewTree{'.'}, '.');

    # Create R files.
    my @RecursiveUpdates = grep {-d $_} @{$Self->{RecursiveUpdates}};

    if (@RecursiveUpdates)
    {
        find(sub
        {
            if (m/^\.svn$/)
            {
                unlink '.svn/ncbi-N';
                unless (-f '.svn/ncbi-R')
                {
                    open FILE, '>.svn/ncbi-R' or confess $!;
                    close FILE
                }
            }
        }, @RecursiveUpdates)
    }
}

sub CollectUpdates
{
    my ($Self, $Dir) = @_;

    return unless -d $Dir;

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

    $Self->PerformUpdates()
}

1
