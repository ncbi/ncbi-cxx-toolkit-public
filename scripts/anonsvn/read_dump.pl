#!/usr/bin/perl -w

use strict;
use warnings;

my (@ExcludedPaths) = @ARGV;

die "Usage: $0 EXCLUDE_PATH ... < DUMPFILE\n" unless @ExcludedPaths;

my %RepositoryTree;

my %CopySources;

# Read the dump and build the repository tree (in %RepositoryTree);
# trace the copyfrom-path history (in %CopySources).
print STDERR "Reading the dumpfile...\n";

{
    my $Path;

    while (<STDIN>)
    {
        chomp;

        if (m/Node-(copyfrom-)?path: (.+)/so)
        {
            if ($1)
            {
                my $CopySource = $2;

                $CopySources{$Path}->{$CopySource} = 1;

                # See if the source path itself or any of its parent
                # directories were also copied from somewhere else.
                my $Container = $CopySource;
                my $RelativePath = '';

                for (;;)
                {
                    if (my $ContainerSources = $CopySources{$Container})
                    {
                        for (keys %$ContainerSources)
                        {
                            $CopySources{$Path}->{$_ . $RelativePath} = 1
                        }
                    }

                    if ($Container =~ m/^(.+)(\/[^\/]+)$/so)
                    {
                        $Container = $1;
                        $RelativePath = $2 . $RelativePath
                    }
                    else
                    {
                        last
                    }
                }
            }
            else
            {
                $Path = $2;

                my $Branch = \%RepositoryTree;

                for my $PathComponent (split('/', $Path))
                {
                    $Branch = ($Branch->{$PathComponent} ||= {})
                }
            }
        }
    }
}

# Mark paths that need to be excluded (specified in the command line)
# with the '/excluded' marker.
ExcludedPath:
for my $ExcludedPath (@ExcludedPaths)
{
    $ExcludedPath =~ s/(?:^\/+|(?<=\/)\/+|\/+$)//sog;

    my $Branch = \%RepositoryTree;

    for my $PathComponent (split('/', $ExcludedPath))
    {
        next ExcludedPath
            if !ref($Branch = $Branch->{$PathComponent}) || $Branch->{'/excluded'}
    }

    $Branch->{'/excluded'} = 1
}

# Exclude copyfrom-path sources from the list of excluded paths.
sub CheckIfCopyTargetIsExcluded
{
    my ($Path, $Sources) = @_;

    my $Branch = \%RepositoryTree;

    for my $PathComponent (split('/', $Path))
    {
        return 0 if !ref($Branch = $Branch->{$PathComponent}) || $Branch->{'/excluded'}
    }

    my $PathIncluded = 0;

    for my $SourcePath (keys %$Sources)
    {
        my $Branch = \%RepositoryTree;

        my @PathComponents = split('/', $SourcePath);

        while (my $PathComponent = shift @PathComponents)
        {
            $Branch = $Branch->{$PathComponent};

            if ($Branch->{'/excluded'})
            {
                $PathIncluded = 1;

                delete $Branch->{'/excluded'};
                $_->{'/excluded'} = 1 for values %$Branch
            }
        }
    }

    return $PathIncluded
}

print STDERR "Including copy sources...\n";

my $PathIncluded;

do
{
    $PathIncluded = 0;

    while (my ($Path, $Sources) = each %CopySources)
    {
        $PathIncluded += CheckIfCopyTargetIsExcluded($Path, $Sources)
    }
}
while ($PathIncluded);

# Build and print the list of the excluded paths.
my @Paths;

sub FindPathsInTree
{
    my ($ParentPath, $Tree) = @_;

    my @Siblings = keys %$Tree;

    while (my ($Name, $Subtree) = each %$Tree)
    {
        my $Path = "$ParentPath/$Name";

        if ($Subtree->{'/excluded'})
        {
            my @MorePathsToExclude = ($Path);

            # Check if $Name is a prefix of any of its siblings.
            for my $Sibling (@Siblings)
            {
                if (length($Sibling) > length($Name) &&
                    substr($Sibling, 0, length($Name)) eq $Name &&
                    !$Tree->{$Sibling}->{'/excluded'})
                {
                    @MorePathsToExclude =
                        map {"$Path/$_"} grep {$_ ne '/excluded'} keys %$Subtree;
                }
            }

            push @Paths, @MorePathsToExclude
        }
        else
        {
            FindPathsInTree($Path, $Subtree)
        }
    }
}

FindPathsInTree('', \%RepositoryTree);

for my $Path (sort @Paths)
{
    $Path =~ s/^\/+//so;
    print "$Path\n"
}
