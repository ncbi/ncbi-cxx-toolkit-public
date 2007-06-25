# $Id$

package NCBI::SVN::Branching;

use base qw(NCBI::SVN::Base);

use strict;
use warnings;
use Carp qw(confess);

use File::Temp qw/tempfile/;
use File::Find;

use NCBI::SVN::Wrapper;
use NCBI::SVN::SwitchMap;
use NCBI::SVN::MultiSwitch;

my $TrunkDir = 'trunk/c++';

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
    my ($RmCommands, $ExistingStructure, $DirName, $TreeToRemove, $Path) = @_;

    if (my $ThisDirStructure = $ExistingStructure->{$DirName})
    {
        if (%$TreeToRemove)
        {
            my @RmCommands;

            while (my ($SubdirName, $Subdir) = each %$TreeToRemove)
            {
                GetRmCommandsR(\@RmCommands, $ThisDirStructure,
                    $SubdirName, $Subdir, "$Path/$SubdirName")
            }

            if (keys %$ThisDirStructure)
            {
                push @$RmCommands, @RmCommands;
                return
            }
        }

        push @$RmCommands, 'rm', $Path;
        delete $ExistingStructure->{$DirName}
    }
}

sub GetRmCommands
{
    my ($RmCommands, $ExistingStructure, $TreeToRemove) = @_;

    while (my ($DirName, $Subdir) = each %$TreeToRemove)
    {
        GetRmCommandsR($RmCommands, $ExistingStructure,
            $DirName, $Subdir, $DirName)
    }
}

sub CreateEntireTree
{
    my ($MkdirCommands, $TreeToCreate, $Path) = @_;

    return unless %$TreeToCreate;

    push @$MkdirCommands, 'mkdir', $Path;

    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        CreateEntireTree($MkdirCommands, $Subdir, "$Path/$DirName")
    }
}

sub GetMkdirCommandsR
{
    my ($MkdirCommands, $ExistingStructure,
        $DirName, $TreeToCreate, $Path) = @_;

    unless ($ExistingStructure->{$DirName})
    {
        $ExistingStructure->{$DirName} = $TreeToCreate;
        return CreateEntireTree($MkdirCommands, $TreeToCreate, $Path)
    }

    $ExistingStructure = $ExistingStructure->{$DirName};

    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        GetMkdirCommandsR($MkdirCommands,
            $ExistingStructure, $DirName, $Subdir, "$Path/$DirName")
    }
}

sub GetMkdirCommands
{
    my ($MkdirCommands, $ExistingStructure, $TreeToCreate) = @_;

    while (my ($DirName, $Subdir) = each %$TreeToCreate)
    {
        GetMkdirCommandsR($MkdirCommands,
            $ExistingStructure, $DirName, $Subdir, $DirName)
    }
}

sub LayPath
{
    my ($Path, @Subdirs) = @_;

    for my $DirName (split('/', $Path))
    {
        for my $Subdir (@Subdirs)
        {
            $Subdir = ($Subdir->{$DirName} ||= {})
        }
    }
}

sub ReadBranchMap
{
    my ($Self, $SVN, $BranchMapRepoPath) = @_;

    my @BranchMapLines = $SVN->ReadFileLines($BranchMapRepoPath);

    return NCBI::SVN::SwitchMap->new(MyName => $Self->{MyName},
            MapFileLines => \@BranchMapLines)
}

sub ReadBranchInfo
{
    my ($Self, $SVN, $BranchPath, $TargetBranchRev, $TargetUpstreamRev) = @_;

    print "Reading branch info...\n";

    $_ ||= 'HEAD' for $TargetBranchRev, $TargetUpstreamRev;

    my $BranchRevisions = $SVN->ReadLog('--stop-on-copy',
        "-r$TargetBranchRev\:1", $SVN->GetRepository(), 'branches/' . $BranchPath);

    my @MergeDownRevisions;

    for my $Revision (@$BranchRevisions)
    {
        if ($Revision->{LogMessage} =~ m/trunk revision (\d+)/o)
        {
            push @MergeDownRevisions, [$Revision, $1]
        }
    }

    my $BranchCreationRevision;
    my $BranchSourceRevision;
    my %BranchStructure;

    my $RevisionIndex = @$BranchRevisions;

    while (--$RevisionIndex >= 0)
    {
        my $Revision = $BranchRevisions->[$RevisionIndex];

        if ($Revision->{LogMessage} =~ m/^Created branch '(.+?)'.$/o &&
            $1 eq $BranchPath)
        {
            $BranchCreationRevision = $Revision;

            for my $Change (@{$Revision->{ChangedPaths}})
            {
                my ($ChangeType, $TargetPath, $SourcePath,
                    $SourceRevision) = @$Change;

                if ($ChangeType eq 'A' && $SourcePath)
                {
                    $BranchStructure{$TargetPath} = $SourcePath;

                    unless ($BranchSourceRevision)
                    {
                        $BranchSourceRevision = $SourceRevision
                    }
                    else
                    {
                        die 'inconsistent branch creation revision'
                            if $BranchSourceRevision != $SourceRevision
                    }
                }
            }

            last
        }
    }

    die 'unable to determine branch source' unless $BranchSourceRevision;

    while (--$RevisionIndex >= 0)
    {
        my $Revision = $BranchRevisions->[$RevisionIndex];

        if ($Revision->{LogMessage} =~ m/^Modified branch '(.+?)'.$/ &&
            $1 eq $BranchPath)
        {
            for my $Change (@{$Revision->{ChangedPaths}})
            {
                my ($ChangeType, $TargetPath, $SourcePath) = @$Change;

                if ($ChangeType eq 'D')
                {
                    delete $BranchStructure{$TargetPath}
                }
                elsif ($ChangeType eq 'A' && $SourcePath)
                {
                    $BranchStructure{$TargetPath} = $SourcePath
                }
            }
        }
    }

    my $CommonTarget = "/branches/$BranchPath/";
    my @BranchDirs;
    my $UpstreamPath;
    my $BranchStructureError = 'incorrect branch structure';

    while (my ($TargetPath, $SourcePath) = each %BranchStructure)
    {
        substr($TargetPath, 0, length($CommonTarget), '') eq $CommonTarget and
            length($SourcePath) > length($TargetPath) and
            substr($SourcePath, -length($TargetPath),
                length($TargetPath), '') eq $TargetPath
            or die $BranchStructureError;

        unless ($UpstreamPath)
        {
            $UpstreamPath = $SourcePath
        }
        else
        {
            $UpstreamPath eq $SourcePath or die $BranchStructureError
        }

        push @BranchDirs, $TargetPath;
    }

    die 'unable to determine the upstream path (branch is empty?)'
        unless $UpstreamPath;

    $UpstreamPath =~ s/^\/?(.+?)\/?$/$1/;

    @BranchDirs = sort @BranchDirs;

    my $FirstBranchRevision = $BranchRevisions->[-1];

    my $UpstreamRevisions = $SVN->ReadLog('--stop-on-copy',
        "-r$TargetUpstreamRev\:$FirstBranchRevision->{Number}",
        $SVN->GetRepository(), map {"$UpstreamPath/$_"} @BranchDirs);

    my @MergeUpRevisions;

    for my $Revision (@$UpstreamRevisions)
    {
        if ($Revision->{LogMessage} =~ m/revision (\d+)/o)
        {
            push @MergeUpRevisions, [$Revision, $1]
        }
    }

    return
    {
        BranchPath => $BranchPath,
        UpstreamPath => $UpstreamPath,
        BranchCreationRevision => $BranchCreationRevision,
        BranchSourceRevision => $BranchSourceRevision,
        BranchDirs => \@BranchDirs,
        BranchRevisions => $BranchRevisions,
        UpstreamRevisions => $UpstreamRevisions,
        MergeDownRevisions => \@MergeDownRevisions,
        MergeUpRevisions => \@MergeUpRevisions
    }
}

sub MakeMergePlan
{
    my ($Self, $SVN, $BranchInfo, $Direction, $TargetRev) = @_;

    my $MergeRevisions = $BranchInfo->{MergeRevisions};

    my $LastTargetRev = scalar(@$MergeRevisions) ?
        $MergeRevisions->[0]->[1] :
        $BranchInfo->{BranchRevisions}->[-1]->{Number};

    if ($TargetRev < $LastTargetRev)
    {
        die "$Self->{MyName}: target revision number must be > $LastTargetRev\n"
    }

    return ["-r$LastTargetRev\:$TargetRev"]
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

    my $Revision = 'HEAD';

    if ($UpstreamPath =~ m/^(.+):(.+)$/)
    {
        $UpstreamPath = $1;
        $Revision = $2
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

    my %MkDirTree;
    my %RmDirTree;
    my %CommonTree;

    my $ExistingBranch;
    # Read the old branch_map, if it exists.
    my $BranchMapRepoPath = "branches/$BranchPath/branch_map";

    my $OldSwitchMap = eval {$Self->ReadBranchMap($SVN, $BranchMapRepoPath)};

    unless ($@)
    {
        die "$Self->{MyName}: branch '$BranchPath' already exists.\n"
    }
    else
    {
        for my $Dir (@BranchDirs)
        {
            my $SourcePath = "$UpstreamPath/$Dir";
            my $TargetPath = "branches/$BranchPath/$Dir";

            LayPath($TargetPath, \%RmDirTree, \%MkDirTree, \%CommonTree);

            push @CopyCommands, 'cp', 'HEAD', $SourcePath, $TargetPath
        }
    }

    my $ExistingStructure = $Self->GetTreeContainingSubtree($SVN,
        $SVN->GetRepository(), \%CommonTree);

    GetRmCommands(\@RmCommands, $ExistingStructure, \%RmDirTree);
    GetMkdirCommands(\@MkdirCommands, $ExistingStructure, \%MkDirTree);

    my @Commands = (@RmCommands, @MkdirCommands, @CopyCommands, @PutCommands);

    # Unless there are no changes, create the branch
    # with a single revision using MUCC.
    if (@Commands)
    {
        print(($ExistingBranch ? 'Updating' : 'Creating') .
            " branch '$BranchPath'...\n");

        system('mucc', '--message', ($ExistingBranch ? 'Modified' : 'Created') .
            " branch '$BranchPath'.", '--root-url', $SVN->{Repos}, @Commands)
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

    my %RmDirTree;

    for (@{$Self->ReadBranchInfo($SVN, $BranchPath)->{BranchDirs}})
    {
        LayPath("branches/$BranchPath/$_", \%RmDirTree)
    }

    GetRmCommands(\@Commands, $Self->GetTreeContainingSubtree($SVN,
        $SVN->GetRepository(), \%RmDirTree), \%RmDirTree);

    # Remove the branch with a single revision using MUCC.
    print("Removing branch '$BranchPath'...\n");

    system('mucc', '--message', "Removed branch '$BranchPath'.",
        '--root-url', $SVN->{Repos}, @Commands);

    unlink $BranchListFN if $BranchListFN;
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $TargetRev) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo;

    if ($Direction eq 'up')
    {
        $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath,
            $TargetRev, undef);

        $TargetRev = $BranchInfo->{BranchRevisions}->[0]->{Number}
    }
    else
    {
        $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath,
            undef, $TargetRev);

        $TargetRev = $BranchInfo->{UpstreamRevisions}->[0]->{Number}
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

    my $LastTargetRev = @$MergeRevisions ? $MergeRevisions->[0]->[1] :
        $Direction ne 'up' ? $BranchInfo->{BranchSourceRevision} :
            $BranchInfo->{BranchCreationRevision}->{Number};

    unless ($TargetRev > $LastTargetRev)
    {
        die "$Self->{MyName}: <target_rev> must be > $LastTargetRev\n"
    }

    my @ExcludedRevisions;

    for my $Revision (@$ReverseMergeRevisions)
    {
        my $RevNumber = $Revision->[0]->{Number};

        last if $RevNumber <= $LastTargetRev;

        push @ExcludedRevisions, $RevNumber
    }

    my @MergePlan;

    for my $Revision (reverse @ExcludedRevisions)
    {
        push @MergePlan, "-r$LastTargetRev\:" . ($Revision - 1)
            if $LastTargetRev < $Revision - 1;

        $LastTargetRev = $Revision
    }

    push @MergePlan, "-r$LastTargetRev\:$TargetRev"
        if $LastTargetRev < $TargetRev;

    unless (@MergePlan)
    {
        print "Nothing to do.\n";
        return
    }

    print "Merging with r$TargetRev...\n";

    my $BaseURL = $SVN->{Repos} . '/' . ($Direction eq 'up' ?
        'branches/' . $BranchPath : $BranchInfo->{UpstreamPath}) . '/';

    for my $LocalDir (@BranchDirs)
    {
        for my $RevRange (@MergePlan)
        {
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

    my $PropValue = 'Please execute "' . $Self->{MyName} .
            ' commit_merge" to commit this merge of branch ' .
            "'$BranchPath' with trunk revision $TargetRev.";

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

sub MergeDownCommit
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchMapRepoPath = "branches/$BranchPath/branch_map";

    my $SwitchMap = $Self->ReadBranchMap($SVN, $BranchMapRepoPath);

    my @BranchDirs = map {$_->[0]} @{$SwitchMap->GetSwitchPlan()};

    my ($TrunkRev) = $SVN->ReadSubversionStream('propget', 'ncbi:raw',
        $BranchDirs[0]) =~ m/trunk revision (\d+)/o;

    die "$Self->{MyName}: unable to detect trunk revision.\n" unless $TrunkRev;

    my @SubDirs;

    find(sub
        {
            if (-d $_ && -d $_ . '/.svn')
            {
                push @SubDirs, $File::Find::name
            }
        }, @BranchDirs);

    system($SVN->GetSvnPath(), 'propdel', 'ncbi:raw', @SubDirs) if @SubDirs;

    system($SVN->GetSvnPath(), 'commit', '-m', "Merged branch '$BranchPath' " .
        "with trunk revision $TrunkRev.", @BranchDirs)
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

sub Switch
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath);

    my @BranchMapLines = map {"$_ branches/$BranchPath/$_"}
        @{$BranchInfo->{BranchDirs}};

    NCBI::SVN::MultiSwitch->new(MyName => $Self->{MyName})->
        SwitchUsingMap(NCBI::SVN::SwitchMap->new(MyName => $Self->{MyName},
            MapFileLines => \@BranchMapLines))
}

sub Unswitch
{
    my ($Self, $BranchPath) = @_;

    die "$Self->{MyName}: <branch_path> parameter is missing\n"
        unless $BranchPath;

    my $SVN = NCBI::SVN::Wrapper->new(MyName => $Self->{MyName});

    my $BranchInfo = $Self->ReadBranchInfo($SVN, $BranchPath);

    my $BaseURL = $SVN->GetRepository() . "/$BranchInfo->{UpstreamPath}/";

    print "Unswitching from branch '$BranchPath'...\n";

    for my $BranchDir (@{$BranchInfo->{BranchDirs}})
    {
        $Self->RunSubversion('switch', $BaseURL . $BranchDir, $BranchDir)
    }
}

1
