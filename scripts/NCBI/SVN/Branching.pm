# $Id$

package NCBI::SVN::Branching;

use base qw(NCBI::SVN::Base);

use strict;
use warnings;
use Carp qw(confess);

use File::Temp qw/tempfile/;
use File::Find;

use NCBI::SVN::Wrapper;

sub GetTreeContainingSubtree
{
    my ($Self, $SVN, $Path, $Subtree) = @_;

    my %Result;

    my $Subdir;

    for my $Entry ($SVN->ReadSubversionLines('list', $Path))
    {
        $Entry =~ s/\/$//o;

        if (ref($Subdir = $Subtree->{$Entry}))
        {
            $Result{$Entry} = keys %$Subdir ?
                $Self->GetTreeContainingSubtree($SVN,
                    "$Path/$Entry", $Subdir) : {}
        }
        else
        {
            $Result{'/moreentries'} = 1
        }
    }

    return \%Result
}

sub GetRmCommandsR
{
    my ($RmCommands, $ExistingStructure, $DirName, $DirContents, $Path) = @_;

    my $DirExistingStructure = $ExistingStructure->{$DirName};

    return 0 unless $DirExistingStructure;

    if (!$DirContents->{'/'}->{rm})
    {
        my $ChildRemoved;
        my @RmCommands;

        while (my ($Subdir, $Subtree) = each %$DirContents)
        {
            next if $Subdir eq '/';

            $ChildRemoved = 1
                if GetRmCommandsR(\@RmCommands, $DirExistingStructure,
                    $Subdir, $Subtree, "$Path/$Subdir")
        }

        if (!$ChildRemoved || %$DirExistingStructure)
        {
            push @$RmCommands, @RmCommands;
            return 0
        }
    }

    delete $ExistingStructure->{$DirName};
    push @$RmCommands, 'rm', $Path;
    return 1
}

sub GetRmCommands
{
    my ($RmCommands, $ExistingStructure, $ModTree) = @_;

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        next if $Subdir eq '/';

        GetRmCommandsR($RmCommands, $ExistingStructure,
            $Subdir, $Subtree, $Subdir)
    }
}

sub CreateEntireTree
{
    my ($MkdirCommands, $ModTree, $Path) = @_;

    return 0 unless %$ModTree;

    my $NeedToCreateThisPath;
    my $NeedToCreateParent;

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        if ($Subdir eq '/')
        {
            $NeedToCreateParent = 1 if $Subtree->{mkparent}
        }
        elsif (CreateEntireTree($MkdirCommands, $Subtree, "$Path/$Subdir"))
        {
            $NeedToCreateParent = $NeedToCreateThisPath = 1
        }
    }

    unshift @$MkdirCommands, 'mkdir', $Path if $NeedToCreateThisPath;

    return $NeedToCreateParent
}

sub GetMkdirCommandsR
{
    my ($MkdirCommands, $ExistingStructure, $ModTree, $Path) = @_;

    unless ($ExistingStructure)
    {
        CreateEntireTree($MkdirCommands, $ModTree, $Path);
        return
    }

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        next if $Subdir eq '/';

        GetMkdirCommandsR($MkdirCommands,
            $ExistingStructure->{$Subdir}, $Subtree, "$Path/$Subdir")
    }
}

sub GetMkdirCommands
{
    my ($MkdirCommands, $ExistingStructure, $ModTree) = @_;

    while (my ($Subdir, $Subtree) = each %$ModTree)
    {
        next if $Subdir eq '/';

        GetMkdirCommandsR($MkdirCommands,
            $ExistingStructure->{$Subdir}, $Subtree, $Subdir)
    }
}

sub FindParentNode
{
    my ($Tree, $Path) = @_;

    my @Subdirs = split('/', $Path);
    my $LastSubdir = pop(@Subdirs);

    for my $Subdir (@Subdirs)
    {
        $Tree = ($Tree->{$Subdir} ||= {})
    }

    return ($Tree, $LastSubdir)
}

sub FindTreeNode
{
    my ($Tree, $Path) = @_;

    for my $Subdir (split('/', $Path))
    {
        $Tree = ($Tree->{$Subdir} ||= {})
    }

    return $Tree
}

sub MarkPath
{
    my ($Path, $Tree, @Markers) = @_;

    $Tree = FindTreeNode($Tree, $Path);

    for my $Marker (@Markers)
    {
        $Tree->{'/'}->{$Marker} = 1
    }
}

sub CutOffCommonTargetPrefix
{
    my ($TargetPath, $CommonTarget, $Revision) = @_;

    substr($TargetPath, 0, length($CommonTarget), '') eq $CommonTarget
        or die "target directory $TargetPath is not " .
            "under $CommonTarget in r$Revision->{Number}";

    return $TargetPath
}

sub SetUpstreamAndSynchRev
{
    my ($BranchInfo, $SourcePath, $BranchDir, $SourceRevision, $Revision) = @_;

    my $UpstreamPath = $SourcePath;

    length($UpstreamPath) > length($BranchDir) and
        substr($UpstreamPath, -length($BranchDir),
            length($BranchDir), '') eq $BranchDir
        or die 'branch structure does not replicate ' .
            "upstream structure in r$Revision->{Number}";

    unless (my $SameUpstreamPath = $BranchInfo->{UpstreamPath})
    {
        $BranchInfo->{UpstreamPath} = $UpstreamPath
    }
    else
    {
        die "inconsistent source paths in r$Revision->{Number}"
            if $UpstreamPath ne $SameUpstreamPath
    }

    unless (my $SingleSourceRevision = $BranchInfo->{LastSynchRevision})
    {
        $BranchInfo->{LastSynchRevision} = $SourceRevision
    }
    else
    {
        die "inconsistent source revision numbers in r$Revision->{Number}"
            if $SourceRevision != $SingleSourceRevision
    }
}

sub ModelBranchStructure
{
    my ($BranchInfo, $BranchStructure, $Revision, $CommonTarget) = @_;

    for my $Change (@{$Revision->{ChangedPaths}})
    {
        my ($ChangeType, $TargetPath, $SourcePath, $SourceRevision) = @$Change;

        if ($ChangeType eq 'D')
        {
            my ($ParentNode, $LastSubdir) = FindParentNode($BranchStructure,
                CutOffCommonTargetPrefix($TargetPath,
                    $CommonTarget, $Revision));

            delete $ParentNode->{$LastSubdir}
        }
        elsif ($ChangeType eq 'R')
        {
            my $BranchDir = CutOffCommonTargetPrefix($TargetPath,
                $CommonTarget, $Revision);

            my ($ParentNode, $LastSubdir) = FindParentNode($BranchStructure,
                $BranchDir);

            unless ($SourcePath)
            {
                delete $ParentNode->{$LastSubdir}
            }
            else
            {
                $ParentNode->{$LastSubdir} = {'/' => 1};

                SetUpstreamAndSynchRev($BranchInfo, $SourcePath,
                    $BranchDir, $SourceRevision)
            }
        }
        elsif ($ChangeType eq 'A' && $SourcePath)
        {
            my $BranchDir = CutOffCommonTargetPrefix($TargetPath,
                $CommonTarget, $Revision);

            FindTreeNode($BranchStructure, $BranchDir)->{'/'} = 1;

            SetUpstreamAndSynchRev($BranchInfo, $SourcePath,
                $BranchDir, $SourceRevision)
        }
    }
}

sub ExtractBranchDirs
{
    my ($BranchDirs, $Path, $Contents) = @_;

    while (my ($Subdir, $Subtree) = each %$Contents)
    {
        unless ($Subdir eq '/')
        {
            ExtractBranchDirs($BranchDirs, "$Path/$Subdir", $Subtree)
        }
        elsif ($Subtree)
        {
            push @$BranchDirs, $Path
        }
    }
}

sub ReadBranchInfo
{
    my ($Self, $SVN, $BranchPath, $MaxBranchRev, $MaxUpstreamRev) = @_;

    print "Reading branch info...\n";

    $_ ||= 'HEAD' for $MaxBranchRev, $MaxUpstreamRev;

    my $RevisionLog = $SVN->ReadLog('--stop-on-copy',
        "-r$MaxBranchRev\:1", $SVN->GetRepository(), 'branches/' . $BranchPath);

    my @BranchRevisions;
    my @MergeDownRevisions;

    my %BranchStructure;
    my @BranchDirs;

    my %BranchInfo =
    (
        BranchPath => $BranchPath,
        BranchDirs => \@BranchDirs,
        BranchRevisions => \@BranchRevisions,
        MergeDownRevisions => \@MergeDownRevisions
    );

    my $CommonTarget = "/branches/$BranchPath/";

    for my $Revision (@$RevisionLog)
    {
        push @BranchRevisions, $Revision;

        if ($Revision->{LogMessage} =~ m/^Created branch '(.+?)'.$/o &&
            $1 eq $BranchPath)
        {
            $BranchInfo{BranchCreationRevision} = $Revision;

            ModelBranchStructure(\%BranchInfo, \%BranchStructure,
                $Revision, $CommonTarget);

            last
        }
    }

    unless ($BranchInfo{UpstreamPath} &&
        ($BranchInfo{BranchSourceRevision} = $BranchInfo{LastSynchRevision}))
    {
        die 'unable to determine branch source'
    }

    my $MergeRegExp =
        qr/^Merged changes up to r(\d+) from '(.+)' into '(.+)'.$/o;

    for my $Revision (reverse @BranchRevisions)
    {
        my $LogMessage = $Revision->{LogMessage};

        if ($LogMessage =~ m/^Modified branch '(.+?)'.$/o && $1 eq $BranchPath)
        {
            ModelBranchStructure(\%BranchInfo, \%BranchStructure,
                $Revision, $CommonTarget)
        }
        elsif ($LogMessage =~ $MergeRegExp && $3 eq $BranchPath)
        {
            unshift @MergeDownRevisions,
                [$Revision, $BranchInfo{LastSynchRevision} = $1]
        }
    }

    while (my ($Dir, $Contents) = each %BranchStructure)
    {
        ExtractBranchDirs(\@BranchDirs, $Dir, $Contents)
    }

    @BranchDirs = sort @BranchDirs;

    $BranchInfo{UpstreamPath} =~ s/^\/?(.+?)\/?$/$1/;
    my $UpstreamPath = $BranchInfo{UpstreamPath};

    my $UpstreamRevisions = $SVN->ReadLog('--stop-on-copy',
        "-r$MaxUpstreamRev\:$BranchRevisions[-1]->{Number}",
        $SVN->GetRepository(), map {"$UpstreamPath/$_"} @BranchDirs);

    my @MergeUpRevisions;

    for my $Revision (@$UpstreamRevisions)
    {
        if ($Revision->{LogMessage} =~ $MergeRegExp && $2 eq $BranchPath)
        {
            push @MergeUpRevisions, [$Revision, $1]
        }
    }

    @BranchInfo{qw(UpstreamRevisions MergeUpRevisions)} =
        ($UpstreamRevisions, \@MergeUpRevisions);

    return \%BranchInfo
}

sub List
{
    my ($Self) = @_;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    print 'Managed branches in ' . $SVN->GetRepository() . ":\n";
    print $SVN->ReadFile('branches/branch_list')
}

sub Info
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath);

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    print "Branch path: branches/$BranchPath\n" .
        "Created: in r$BranchInfo->{BranchCreationRevision}->{Number}" .
        " from $UpstreamPath\@$BranchInfo->{BranchSourceRevision}\n";

    print "Branch structure:\n";
    for my $Directory (@{$BranchInfo->{BranchDirs}})
    {
        print "  $Directory\n"
    }

    sub Times
    {
        return @{$_[0]} ? @{$_[0]} > 1 ? @{$_[0]} . ' times' : 'once' : 'never'
    }

    print "Merged down into $BranchPath\: " .
        Times($BranchInfo->{MergeDownRevisions}) . "\n";

    for my $RevRef (@{$BranchInfo->{MergeDownRevisions}})
    {
        print "  r$RevRef->[0]->{Number} (from $UpstreamPath\@$RevRef->[1])\n"
    }

    print "Merged up into $UpstreamPath\: " .
        Times($BranchInfo->{MergeUpRevisions}) . "\n";

    for my $RevRef (@{$BranchInfo->{MergeUpRevisions}})
    {
        print "  r$RevRef->[0]->{Number} (from $BranchPath\@$RevRef->[1])\n"
    }
}

sub Create
{
    my ($Self, $BranchPath, $UpstreamPath, @BranchDirs) = @_;

    for ([branch_path => $BranchPath], [upstream_path => $UpstreamPath],
        [branch_dirs => $BranchDirs[0]])
    {
        die "$Self->{MyName}: <$_->[0]> parameter is missing\n" unless $_->[1]
    }

    my $SourceRevision = 'HEAD';

    if ($UpstreamPath =~ m/^(.+):(.+)$/)
    {
        $UpstreamPath = $1;
        $SourceRevision = $2
    }

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my @RmCommands;
    my @MkdirCommands;
    my @CopyCommands;
    my @PutCommands;

    my $BranchListFN;

    # Read branch_list, if it exists.
    my @ExistingBranches = eval {$SVN->ReadFileLines('branches/branch_list')};

    # Unless branch_list contains our branch path already, add a MUCC
    # command to (re-)create branch_list.
    if ($@ || !grep {$_ eq $BranchPath} @ExistingBranches)
    {
        my $BranchListFH;

        ($BranchListFH, $BranchListFN) = tempfile();

        print $BranchListFH "$_\n" for @ExistingBranches, $BranchPath;

        close $BranchListFH;

        push @PutCommands, 'put', $BranchListFN, 'branches/branch_list'
    }
    else
    {
        die "$Self->{MyName}: branch '$BranchPath' already exists.\n";
    }

    my %ModTree;

    for my $Dir (@BranchDirs)
    {
        my $SourcePath = "$UpstreamPath/$Dir";
        my $TargetPath = "branches/$BranchPath/$Dir";

        MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
        push @CopyCommands, 'cp', $SourceRevision, $SourcePath, $TargetPath
    }

    my $ExistingStructure = $Self->GetTreeContainingSubtree($SVN,
        $SVN->GetRepository(), \%ModTree);

    GetRmCommands(\@RmCommands, $ExistingStructure, \%ModTree);
    GetMkdirCommands(\@MkdirCommands, $ExistingStructure, \%ModTree);

    my @Commands = (@RmCommands, @MkdirCommands, @CopyCommands, @PutCommands);

    # Unless there are no changes, create the branch
    # with a single revision using MUCC.
    if (@Commands)
    {
        print "Creating branch '$BranchPath'...\n";

        system('mucc', '--message', "Created branch '$BranchPath'.",
            '--root-url', $SVN->{Repos}, @Commands)
    }
    else
    {
        print "Nothing to do.\n"
    }

    unlink $BranchListFN if $BranchListFN;
}

sub Alter
{
    my ($Self, $BranchPath, @BranchDirs) = @_;

    for ([branch_path => $BranchPath], [branch_dirs => $BranchDirs[0]])
    {
        die "$Self->{MyName}: <$_->[0]> parameter is missing\n" unless $_->[1]
    }

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath);

    my $MergeRevisions = $BranchInfo->{MergeDownRevisions};
    my $LastSynchRev = $BranchInfo->{LastSynchRevision};
    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    my @RmCommands;
    my @MkdirCommands;
    my @CopyCommands;
    my @PutCommands;

    my $BranchListFN;

    # Read branch_list, if it exists.
    my @ExistingBranches = eval {$SVN->ReadFileLines('branches/branch_list')};

    # Unless branch_list contains our branch path already, add a MUCC
    # command to (re-)create branch_list.
    if ($@ || !grep {$_ eq $BranchPath} @ExistingBranches)
    {
        my $BranchListFH;

        ($BranchListFH, $BranchListFN) = tempfile();

        print $BranchListFH "$_\n" for @ExistingBranches, $BranchPath;

        close $BranchListFH;

        push @PutCommands, 'put', $BranchListFN, 'branches/branch_list'
    }

    my %OldBranchDirs = map {$_ => 1} @{$BranchInfo->{BranchDirs}};

    my %ModTree;

    for my $Dir (@BranchDirs)
    {
        unless (delete $OldBranchDirs{$Dir})
        {
            my $SourcePath = "$UpstreamPath/$Dir";
            my $TargetPath = "branches/$BranchPath/$Dir";

            MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
            push @CopyCommands, 'cp', $LastSynchRev, $SourcePath, $TargetPath
        }
    }

    for my $Dir (keys %OldBranchDirs)
    {
        MarkPath("branches/$BranchPath/$Dir", \%ModTree, 'rm')
    }

    my $ExistingStructure = $Self->GetTreeContainingSubtree($SVN,
        $SVN->GetRepository(), \%ModTree);

    GetRmCommands(\@RmCommands, $ExistingStructure, \%ModTree);
    GetMkdirCommands(\@MkdirCommands, $ExistingStructure, \%ModTree);

    my @Commands = (@RmCommands, @MkdirCommands, @CopyCommands, @PutCommands);

    # Unless there are no changes, alter the branch
    # with a single revision using MUCC.
    if (@Commands)
    {
        print "Updating branch '$BranchPath'...\n";

        system('mucc', '--message', "Modified branch '$BranchPath'.",
            '--root-url', $SVN->{Repos}, @Commands)
    }
    else
    {
        print "Nothing to do.\n"
    }

    unlink $BranchListFN if $BranchListFN;
}

sub Remove
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my @Commands;

    my $BranchListFN;

    # Read branch_list, if it exists.
    my @Branches = eval {$SVN->ReadFileLines('branches/branch_list')};

    if ($@)
    {
        warn "Warning: 'branches/branch_list' was not found.\n"
    }
    else
    {
        my @NewBranches = grep {$_ ne $BranchPath} @Branches;

        if (scalar(@NewBranches) != scalar(@Branches))
        {
            my $BranchListFH;

            ($BranchListFH, $BranchListFN) = tempfile();

            print $BranchListFH "$_\n" for @NewBranches;

            close $BranchListFH;

            push @Commands, 'put', $BranchListFN, 'branches/branch_list'
        }
        else
        {
            warn 'Warning: branch ' .
                "'$BranchPath' was not found in branch_list.\n";
        }
    }

    my %ModTree;

    for (@{$Self->ReadBranchInfo($SVN, $BranchPath)->{BranchDirs}})
    {
        MarkPath("branches/$BranchPath/$_", \%ModTree, 'rm')
    }

    GetRmCommands(\@Commands, $Self->GetTreeContainingSubtree($SVN,
        $SVN->GetRepository(), \%ModTree), \%ModTree);

    # Remove the branch with a single revision using MUCC.
    print("Removing branch '$BranchPath'...\n");

    system('mucc', '--message', "Removed branch '$BranchPath'.",
        '--root-url', $SVN->{Repos}, @Commands);

    unlink $BranchListFN if $BranchListFN;
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $SourceRev) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo;

    if ($Direction eq 'up')
    {
        $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath,
            $SourceRev, undef);

        $SourceRev = $BranchInfo->{BranchRevisions}->[0]->{Number}
    }
    else
    {
        $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath,
            undef, $SourceRev);

        my $UpstreamRevisions = $BranchInfo->{UpstreamRevisions};

        $SourceRev = @$UpstreamRevisions ? $UpstreamRevisions->[0]->{Number} :
            $BranchInfo->{BranchRevisions}->[-1]->{Number} - 1;
    }

    my @BranchDirs = @{$BranchInfo->{BranchDirs}};

    print "Running svn status on branch directories...\n";

    for ($SVN->ReadSubversionLines('status', @BranchDirs))
    {
        die "$Self->{MyName}: local modifications detected.\n"
            unless m/^[\?~X]/
    }

    print "Performing updates...\n";

    system($SVN->GetSvnPath(), 'update', @BranchDirs);

    my ($MergeRevisions, $ReverseMergeRevisions) =
        @$BranchInfo{$Direction eq 'up' ?
            qw(MergeUpRevisions MergeDownRevisions) :
            qw(MergeDownRevisions MergeUpRevisions)};

    my $LastSourceRev = @$MergeRevisions ? $MergeRevisions->[0]->[1] :
        $Direction ne 'up' ? $BranchInfo->{BranchSourceRevision} :
            $BranchInfo->{BranchCreationRevision}->{Number};

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    my ($SourcePath, $TargetPath) = $Direction ne 'up' ?
        ($UpstreamPath, $BranchPath) : ($BranchPath, $UpstreamPath);

    unless ($SourceRev > $LastSourceRev)
    {
        die "$Self->{MyName}: '$TargetPath' is already " .
            "in synch with r$LastSourceRev of '$SourcePath'\n"
    }

    my @ExcludedRevisions;

    for my $Revision (@$ReverseMergeRevisions)
    {
        my $RevNumber = $Revision->[0]->{Number};

        last if $RevNumber <= $LastSourceRev;

        push @ExcludedRevisions, $RevNumber
    }

    my @MergePlan;

    for my $Revision (reverse @ExcludedRevisions)
    {
        push @MergePlan, "-r$LastSourceRev\:" . ($Revision - 1)
            if $LastSourceRev < $Revision - 1;

        $LastSourceRev = $Revision
    }

    push @MergePlan, "-r$LastSourceRev\:$SourceRev"
        if $LastSourceRev < $SourceRev;

    unless (@MergePlan)
    {
        print "Nothing to do.\n";
        return
    }

    print "Merging with r$SourceRev...\n";

    my $BaseURL = $SVN->{Repos} . '/' . ($Direction eq 'up' ?
        'branches/' . $BranchPath : $UpstreamPath) . '/';

    for my $LocalDir (@BranchDirs)
    {
        for my $RevRange (@MergePlan)
        {
            print "  $RevRange => $LocalDir\n";
            system($SVN->GetSvnPath(), 'merge', $RevRange,
                $BaseURL . $LocalDir, $LocalDir)
        }
    }

    my @RootDirs;
    my @SubDirs;

    find(sub
        {
            if ($_ eq '.')
            {
                push @RootDirs, $File::Find::name
            }
            elsif (-d $_ && -d $_ . '/.svn')
            {
                push @SubDirs, $File::Find::name
            }
        }, @BranchDirs);

    my $PropValue = qq(Please run "$Self->{MyName} commit_merge" to merge ) .
            "changes up to r$SourceRev from '$SourcePath' into '$TargetPath'.";

    system($SVN->GetSvnPath(), 'propset', 'ncbi:raw', $PropValue, @RootDirs);

    system($SVN->GetSvnPath(), 'propset', 'ncbi:raw', $PropValue . ' [subdir]',
        @SubDirs) if @SubDirs
}

sub MergeDown
{
    my $Self = shift;

    return $Self->DoMerge('down', @_)
}

sub MergeUp
{
    my $Self = shift;

    return $Self->DoMerge('up', @_)
}

sub CommitMerge
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath, undef, undef);

    my @BranchDirs = @{$BranchInfo->{BranchDirs}};

    my $Message;

    for my $Dir (@BranchDirs)
    {
        my $PropValue = $SVN->ReadSubversionStream('propget', 'ncbi:raw', $Dir);

        unless ($Message)
        {
            $Message = $PropValue
        }
        elsif ($Message ne $PropValue)
        {
            die "$Self->{MyName}: wrong cwd for the commit_merge operation.\n"
        }
    }

    my ($Changes) = $Message =~ m/to merge (changes .*)$/o;

    die "$Self->{MyName}: cannot retrieve log message.\n" unless $Changes;

    my @SubDirs;

    find(sub
        {
            if (-d $_ && -d $_ . '/.svn')
            {
                push @SubDirs, $File::Find::name
            }
        }, @BranchDirs);

    system($SVN->GetSvnPath(), 'propdel', 'ncbi:raw', @SubDirs) if @SubDirs;

    system($SVN->GetSvnPath(), 'commit', '-m', "Merged $Changes", @BranchDirs)
}

sub Svn
{
    my ($Self, $BranchPath, @CommandAndArgs) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    exec($SVN->GetSvnPath(), @CommandAndArgs,
        @{$Self->ReadBranchInfo($SVN, $BranchPath)->{BranchDirs}})
}

sub DoSwitchUnswitch
{
    my ($Self, $BranchPath, $DoSwitch) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath);

    print(($DoSwitch ? 'Switching to' : 'Unswitching from') .
        " branch '$BranchPath'...\n");

    my $BaseURL = $SVN->GetRepository() . '/' . ($DoSwitch ?
        "branches/$BranchPath" : $BranchInfo->{UpstreamPath}) . '/';

    for (@{$BranchInfo->{BranchDirs}})
    {
        $Self->RunSubversion('switch', $BaseURL . $_, $_)
    }
}

sub Switch
{
    DoSwitchUnswitch(@_, 1)
}

sub Unswitch
{
    DoSwitchUnswitch(@_, 0)
}

1
