# $Id$

package NCBI::SVN::Branching::BranchShapingOps;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use NCBI::SVN::Branching::Util;
use NCBI::SVN::Branching::BranchInfo;

use File::Temp qw/tempfile/;

sub MarkPath
{
    my ($Path, $Tree, @Markers) = @_;

    $Tree = NCBI::SVN::Branching::Util::FindTreeNode($Tree, $Path);

    for my $Marker (@Markers)
    {
        $Tree->{'/'}->{$Marker} = 1
    }
}

sub GetTreeContainingSubtree
{
    my ($Self, $Path, $Subtree) = @_;

    my %Result;

    my $Subdir;

    for my $Entry ($Self->{SVN}->ReadSubversionLines('list', $Path))
    {
        $Entry =~ s/\/$//o;

        if (ref($Subdir = $Subtree->{$Entry}))
        {
            $Result{$Entry} = keys %$Subdir ?
                $Self->GetTreeContainingSubtree("$Path/$Entry", $Subdir) : {}
        }
        else
        {
            $Result{'/moreentries'} = 1
        }
    }

    return \%Result
}

sub GetPathsToRemove
{
    my ($PathsToRemove, $ExistingStructure, $DirName, $DirContents, $Path) = @_;

    my $DirExistingStructure = $ExistingStructure->{$DirName};

    return 0 unless $DirExistingStructure;

    if (!$DirContents->{'/'}->{rm})
    {
        my $ChildRemoved;
        my @PathsToRemove;

        while (my ($Subdir, $Subtree) = each %$DirContents)
        {
            next if $Subdir eq '/';

            $ChildRemoved = 1
                if GetPathsToRemove(\@PathsToRemove, $DirExistingStructure,
                    $Subdir, $Subtree, "$Path/$Subdir")
        }

        if (!$ChildRemoved || %$DirExistingStructure)
        {
            push @$PathsToRemove, @PathsToRemove;
            return 0
        }
    }

    delete $ExistingStructure->{$DirName};
    push @$PathsToRemove, $Path;
    return 1
}

sub CreateEntireTree
{
    my ($DirsToCreate, $ModTree, $Path) = @_;

    return 0 unless %$ModTree;

    my $NeedToCreateThisPath;
    my $NeedToCreateParent;

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        if ($Subdir eq '/')
        {
            $NeedToCreateParent = 1 if $Subtree->{mkparent}
        }
        elsif (CreateEntireTree($DirsToCreate, $Subtree, "$Path/$Subdir"))
        {
            $NeedToCreateParent = $NeedToCreateThisPath = 1
        }
    }

    unshift @$DirsToCreate, $Path if $NeedToCreateThisPath;

    return $NeedToCreateParent
}

sub GetDirsToCreate
{
    my ($DirsToCreate, $ExistingStructure, $ModTree, $Path) = @_;

    unless ($ExistingStructure)
    {
        CreateEntireTree($DirsToCreate, $ModTree, $Path);
        return
    }

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        next if $Subdir eq '/';

        GetDirsToCreate($DirsToCreate,
            $ExistingStructure->{$Subdir}, $Subtree, "$Path/$Subdir")
    }
}

sub ShapeBranch
{
    my ($Self, $Action, $DryRun, $Force, $RootURL, $BranchPath,
        $UpstreamPath, $SourceRevision, @BranchPaths) = @_;

    my @MUCCCommands;
    my %ModTree;
    my $RewriteBranchList;

    # Read branch_list, if it exists.
    my @Branches = eval {$Self->{SVN}->ReadFileLines($RootURL .
        '/branches/branch_list')};

    if ($Action eq 'create')
    {
        if ($@)
        {
            warn "Warning: 'branches/branch_list' was not found.\n"
        }
        elsif (grep {$_ eq $BranchPath} @Branches)
        {
            die "$Self->{MyName}: branch '$BranchPath' already exists.\n"
        }

        push @Branches, $BranchPath;
        $RewriteBranchList = 1;

        $SourceRevision ||= 'HEAD';

        for my $Path (@BranchPaths)
        {
            my $SourcePath = "$UpstreamPath/$Path";
            my $TargetPath = "$BranchPath/$Path";

            MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
            push @MUCCCommands, 'cp', $SourceRevision, $SourcePath, $TargetPath
        }
    }
    elsif ($Action eq 'alter' or $Action eq 'grow' or $Action eq 'truncate')
    {
        unless (grep {$_ eq $BranchPath} @Branches)
        {
            warn 'Warning: branch ' .
                "'$BranchPath' was not found in branch_list.\n";

            push @Branches, $BranchPath;
            $RewriteBranchList = 1
        }

        my $BranchInfo =
            NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

        my %OldBranchPaths = map {$_ => 1} @{$BranchInfo->{BranchPaths}};

        if ($Action ne 'truncate')
        {
            $UpstreamPath = $BranchInfo->{UpstreamPath};

            $SourceRevision ||= $BranchInfo->{LastDownSyncRevisionNumber};

            for my $Path (@BranchPaths)
            {
                unless (delete $OldBranchPaths{$Path})
                {
                    my $SourcePath = "$UpstreamPath/$Path";
                    my $TargetPath = "$BranchPath/$Path";

                    MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
                    push @MUCCCommands, 'cp',
                        $SourceRevision, $SourcePath, $TargetPath
                }
            }

            if ($Action eq 'alter')
            {
                for my $Path (keys %OldBranchPaths)
                {
                    MarkPath("$BranchPath/$Path", \%ModTree, 'rm')
                }
            }
        }
        else
        {
            for my $Path (@BranchPaths)
            {
                MarkPath("$BranchPath/$Path", \%ModTree, 'rm')
                    if delete $OldBranchPaths{$Path}
            }

            die "$Self->{MyName}: cannot remove all branch elements.\n"
                unless %OldBranchPaths
        }
    }
    elsif ($Action eq 'remove')
    {
        my @NewBranchList = grep {$_ ne $BranchPath} @Branches;

        if (scalar(@NewBranchList) != scalar(@Branches))
        {
            @Branches = @NewBranchList;
            $RewriteBranchList = 1
        }

        if (my $BranchInfo =
            eval {NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath)})
        {
            my $BranchRevisions = $BranchInfo->{BranchRevisions};
            my $UpstreamInfo;

            if (!$Force && @$BranchRevisions > 1 && ($UpstreamInfo =
                eval {NCBI::SVN::Branching::UpstreamInfo($BranchInfo)}) &&
                $BranchRevisions->[0]->{Number} >
                    ($UpstreamInfo->{MergeUpRevisions}->[0]->
                        {SourceRevisionNumber} || 0))
            {
                die "WARNING: Latest modifications in '$BranchPath'\n" .
                    "were not merged into '$BranchInfo->{UpstreamPath}'.\n" .
                    "Use --force to remove the branch anyway.\n"
            }

            for (@{$BranchInfo->{BranchPaths}})
            {
                MarkPath("$BranchPath/$_", \%ModTree, 'rm')
            }
        }
        elsif (!$Force)
        {
            die "$Self->{MyName}: cannot retrieve branch information.\n" .
                "Use --force to remove branch record from 'branch_list'.\n"
        }
    }

    my $ExistingStructure =
        $Self->GetTreeContainingSubtree($RootURL, \%ModTree);

    my @PathsToRemove;
    my @DirsToCreate;

    while (my ($Subdir, $Subtree) = each %ModTree)
    {
        next if $Subdir eq '/';

        GetPathsToRemove(\@PathsToRemove, $ExistingStructure,
            $Subdir, $Subtree, $Subdir)
    }

    unless ($Action eq 'remove' or $Action eq 'truncate')
    {
        if (!$Force && @PathsToRemove)
        {
            print STDERR 'WARNING: The requested repository modification ' .
                "will result in\nthe following path(s) being removed:\n  " .
                join("\n  ", @PathsToRemove) .
                "\nRerun with --force to perform the modification anyway.\n";

            return
        }

        while (my ($Subdir, $Subtree) = each %ModTree)
        {
            next if $Subdir eq '/';

            GetDirsToCreate(\@DirsToCreate,
                $ExistingStructure->{$Subdir}, $Subtree, $Subdir)
        }
    }

    unshift @MUCCCommands,
        map({(rm => $_)} @PathsToRemove),
        map({(mkdir => $_)} @DirsToCreate);

    my $BranchListFN;

    # If the current operation affects branch_list, add a MUCC
    # command to (re-)create branch_list.
    if ($RewriteBranchList)
    {
        my $BranchListFH;

        ($BranchListFH, $BranchListFN) = tempfile();

        print $BranchListFH "$_\n" for @Branches;

        close $BranchListFH;

        push @MUCCCommands, 'put', $BranchListFN, 'branches/branch_list'
    }

    # Unless there are no changes, (re)shape or remove the branch
    # with a single revision using MUCC.
    if (@MUCCCommands)
    {
        my ($VerbING, $VerbED) = $Action eq 'create' ? qw(Creating Created) :
            $Action eq 'remove' ? qw(Removing Removed) : qw(Updating Modified);

        print STDERR "$VerbING branch '$BranchPath'...\n";

        unshift @MUCCCommands, '--root-url', $RootURL,
            '--message', "$VerbED branch '$BranchPath'.";

        unless ($DryRun)
        {
            $Self->{SVN}->RunMUCC(@MUCCCommands)
        }
        else
        {
            print "*** DRY RUN ***\nsvnmucc\n";
            print "$_\n" for @MUCCCommands
        }

        unless ($DryRun or $Action eq 'remove' or $Action eq 'create')
        {
            print STDERR 'WARNING: The branch has been modified. Please ' .
                "run\n  $Self->{MyName} update " .
                NCBI::SVN::Branching::Util::SimplifyBranchPath($BranchPath) .
                "\nin all working copies switched to this branch.\n"
        }
    }
    else
    {
        print STDERR "Nothing to do.\n"
    }

    unlink $BranchListFN if $BranchListFN
}

sub Create
{
    my ($Self, $DryRun, $RootURL, $BranchPath,
        $UpstreamPath, $SourceRev, @BranchPaths) = @_;

    $Self->ShapeBranch('create', $DryRun, undef, $RootURL,
        $BranchPath, $UpstreamPath, $SourceRev, @BranchPaths)
}

sub Alter
{
    my ($Self, $DryRun, $Force, $RootURL, $BranchPath, @BranchPaths) = @_;

    $Self->ShapeBranch('alter', $DryRun, $Force, $RootURL,
        $BranchPath, undef, undef, @BranchPaths)
}

sub Grow
{
    my ($Self, $DryRun, $RootURL, $BranchPath, $SourceRev, @BranchPaths) = @_;

    $Self->ShapeBranch('grow', $DryRun, undef, $RootURL,
        $BranchPath, undef, $SourceRev, @BranchPaths)
}

sub Truncate
{
    my ($Self, $DryRun, $RootURL, $BranchPath, @BranchPaths) = @_;

    $Self->ShapeBranch('truncate', $DryRun, undef, $RootURL,
        $BranchPath, undef, undef, @BranchPaths)
}

sub Remove
{
    my ($Self, $DryRun, $Force, $RootURL, $BranchPath) = @_;

    $Self->ShapeBranch('remove', $DryRun, $Force, $RootURL, $BranchPath)
}

1
