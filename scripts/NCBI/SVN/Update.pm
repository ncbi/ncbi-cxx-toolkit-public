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
    my ($Self, $DirsToUpdate, $Dir) = @_;

    my $RFile = "$Dir/.svn/ncbi-R";

    unless (-f $RFile)
    {
        print "L $Dir\n";

        my @SubDirsToUpdate;

        for my $SubDir ($Self->{SVN}->ReadSubversionLines('list', $Dir))
        {
            if ($SubDir =~ m/^(.+)\/$/os)
            {
                $SubDir = "$Dir/$1";

                unless (-d $SubDir)
                {
                    push @SubDirsToUpdate, $1
                }
                else
                {
                    $Self->SwitchToRecursive($DirsToUpdate, $SubDir)
                }
            }
        }

        push @$DirsToUpdate, [$Dir, @SubDirsToUpdate] if @SubDirsToUpdate
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

    return unless ref($Branch) || -f "$Path/.svn/ncbi-R";

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

sub CheckOutMissingExternals
{
    my ($Self, $NonRecursive, $ExistingSubDirs,
        $NewExternalSubDirs, $Path, @SubDirs) = @_;

    my @MissingSubDirs;

    for my $SubDir (@SubDirs)
    {
        unless (-d "$Path/$SubDir")
        {
            push @MissingSubDirs, $SubDir
        }
        elsif ($ExistingSubDirs)
        {
            push @$ExistingSubDirs, $SubDir
        }
    }

    if (@MissingSubDirs)
    {
        my %ExternalURL;

        for ($Self->{SVN}->ReadSubversionLines(qw(pg svn:externals), $Path))
        {
            if (my ($SubDir, $URL) = m/^\s*(.+?)\s+(?:-r\s*\d+\s+)?(.+?)\s*$/so)
            {
                $ExternalURL{$SubDir} = $URL
            }
        }

        for my $SubDir (@MissingSubDirs)
        {
            my $SubDirPath = "$Path/$SubDir";

            if (my $URL = $ExternalURL{$SubDir})
            {
                print "Checking out external '$SubDirPath':\n";

                $Self->{SVN}->RunSubversion(
                    ($NonRecursive ? qw(co -N) : qw(co)), $URL, $SubDirPath);

                push @$NewExternalSubDirs, $SubDir if $NewExternalSubDirs
            }
            else
            {
                warn "WARNING: Could not check out '$SubDirPath'\n"
            }
        }
    }
}

sub CheckForNonRecursiveExternals
{
    my ($Self, $Path, $Branch) = @_;

    if (ref $Branch)
    {
        my (@ExistingSubDirs, @NewExternalSubDirs);

        $Self->CheckOutMissingExternals('non-recursive',
            \@ExistingSubDirs, \@NewExternalSubDirs, $Path, keys %$Branch);

        $Self->CheckForNonRecursiveExternals("$Path/$_", $Branch->{$_})
            for @ExistingSubDirs;

        $Self->PerformNonRecursiveUpdates("$Path/$_", $Branch->{$_})
            for @NewExternalSubDirs
    }
}

sub PerformNonRecursiveUpdates
{
    my ($Self, $Path, $Tree) = @_;

    sub BuildNonRecDirLists
    {
        my ($DirList, $Path, $Branch) = @_;

        if (ref $Branch)
        {
            push @$DirList, $Path;

            while (my ($SubDir, $SubBranch) = each %$Branch)
            {
                BuildNonRecDirLists($DirList, "$Path/$SubDir", $SubBranch)
            }
        }
    }

    my @DirList;

    BuildNonRecDirLists(\@DirList, $Path, $Tree);

    if (@DirList)
    {
        print "Performing non-recursive updates" .
            ($Path eq '.' ? ":\n" : " (in '$Path'):\n");

        $Self->{SVN}->RunSubversion(qw(update -N), @DirList);

        $Self->CheckForNonRecursiveExternals($Path, $Tree)
    }
}

sub BuildListOfDirsToUpdateRecursively
{
    my ($Self, $DirsToUpdate, $Path, $Branch) = @_;

    my @SubDirsToUpdate;

    while (my ($SubDir, $SubBranch) = each %$Branch)
    {
        unless (ref $SubBranch)
        {
            push @SubDirsToUpdate, $SubDir;

            my $SubDirPath = "$Path/$SubDir";

            $Self->SwitchToRecursive($DirsToUpdate, $SubDirPath)
                if -d $SubDirPath
        }
        else
        {
            $Self->BuildListOfDirsToUpdateRecursively($DirsToUpdate,
                "$Path/$SubDir", $SubBranch)
        }
    }

    push @$DirsToUpdate, [$Path, @SubDirsToUpdate] if @SubDirsToUpdate
}

sub UpdateDirList
{
    my ($Self, @Paths) = @_;

    confess 'List of directories to update is empty' unless @Paths;
    confess 'Not in a working copy directory' unless -d '.svn';

    my %NewTree;

Path:
    for my $Path (@Paths)
    {
        $Path =~ m/(?:^|\/)\.\.(?:\/|$)/o &&
            die "$Self->{MyName}: path '$Path' contains '..'\n";

        $Path =~ s/\/\.(?=\/|$)//go;

        $Path =~ s/^(?:\.\/)+//o;

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

    $Self->PerformNonRecursiveUpdates('.', \%NewTree);

    $Self->CreateNFiles(\%NewTree, '.');

    my @DirsToUpdate;

    $Self->BuildListOfDirsToUpdateRecursively(\@DirsToUpdate, '.', \%NewTree);

    if (@DirsToUpdate)
    {
        my @DirList;

        print "Performing recursive updates:\n";

        for (@DirsToUpdate)
        {
            my ($Path, @SubDirs) = @$_;

            push @DirList, map {"$Path/$_"} @SubDirs
        }

        $Self->{SVN}->RunSubversion('update', @DirList);

        # Check for missing externals.
        $Self->CheckOutMissingExternals(0, undef, undef, @$_) for @DirsToUpdate;

        # Create R files.
        my @RecursiveUpdates = grep {-d $_} @DirList;

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

    if (@{$Self->{NonRecursiveUpdates}})
    {
        print "Performing non-recursive updates:\n";

        $Self->{SVN}->RunSubversion(qw(update -N),
            @{$Self->{NonRecursiveUpdates}})
    }

    if (@{$Self->{RecursiveUpdates}})
    {
        print "Performing recursive updates:\n";

        $Self->{SVN}->RunSubversion('update', @{$Self->{RecursiveUpdates}})
    }
}

1
