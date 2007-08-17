# $Id$

package NCBI::SVN::Branching;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use File::Temp qw/tempfile/;

use NCBI::SVN::Branching::Util;
use NCBI::SVN::Branching::BranchInfo;
use NCBI::SVN::Wrapper;

sub List
{
    my ($Self, $RootURL) = @_;

    print "Managed branches in $RootURL\:\n";

    print NCBI::SVN::Branching::Util::SimplifyBranchPath($_) . "\n"
        for $Self->{SVN}->ReadFileLines($RootURL . '/branches/branch_list')
}

sub Info
{
    my ($Self, $RootURL, $BranchPath) = @_;

    my $BranchInfo =
        NCBI::SVN::Branching::BranchAndUpstreamInfo->new($RootURL, $BranchPath);

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    print "Branch path: $BranchPath\n" .
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

    print "Merged from '$UpstreamPath' into '$BranchPath': " .
        Times($BranchInfo->{MergeDownRevisions}) . "\n";

    for my $RevRef (@{$BranchInfo->{MergeDownRevisions}})
    {
        print "  r$RevRef->[0]->{Number} (from $UpstreamPath\@$RevRef->[1])\n"
    }

    print "Merged from '$BranchPath' into '$UpstreamPath': " .
        Times($BranchInfo->{MergeUpRevisions}) . "\n";

    for my $RevRef (@{$BranchInfo->{MergeUpRevisions}})
    {
        print "  r$RevRef->[0]->{Number} (from $BranchPath\@$RevRef->[1])\n"
    }
}

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
    my ($Self, $Action, $Force, $RootURL,
        $BranchPath, $UpstreamPath, @BranchDirs) = @_;

    my @MUCCCommands;

    my @PathsToRemove;
    my @DirsToCreate;

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
        $RewriteBranchList = 1
    }
    elsif ($Action eq 'alter')
    {
        unless (grep {$_ eq $BranchPath} @Branches)
        {
            warn 'Warning: branch ' .
                "'$BranchPath' was not found in branch_list.\n";

            push @Branches, $BranchPath;
            $RewriteBranchList = 1
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
    }

    my %ModTree;

    if ($Action eq 'create')
    {
        my $SourceRevision = 'HEAD';

        if ($UpstreamPath =~ m/^(.+):(.+)$/)
        {
            $UpstreamPath = $1;
            $SourceRevision = $2
        }

        for my $Dir (@BranchDirs)
        {
            my $SourcePath = "$UpstreamPath/$Dir";
            my $TargetPath = "$BranchPath/$Dir";

            MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
            push @MUCCCommands, 'cp', $SourceRevision, $SourcePath, $TargetPath
        }
    }
    elsif ($Action eq 'alter')
    {
        my $BranchInfo =
            NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

        $UpstreamPath = $BranchInfo->{UpstreamPath};

        my %OldBranchDirs = map {$_ => 1} @{$BranchInfo->{BranchDirs}};

        for my $Dir (@BranchDirs)
        {
            unless (delete $OldBranchDirs{$Dir})
            {
                my $SourcePath = "$UpstreamPath/$Dir";
                my $TargetPath = "$BranchPath/$Dir";

                MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
                push @MUCCCommands, 'cp', $BranchInfo->{LastSynchRevision},
                    $SourcePath, $TargetPath
            }
        }

        for my $Dir (keys %OldBranchDirs)
        {
            MarkPath("$BranchPath/$Dir", \%ModTree, 'rm')
        }
    }
    elsif ($Action eq 'remove')
    {
        my $BranchInfo;

        if ($Force)
        {
            $BranchInfo =
                NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath)
        }
        else
        {
            $BranchInfo =
                NCBI::SVN::Branching::BranchAndUpstreamInfo->new($RootURL,
                    $BranchPath);

            my $BranchRevisions = $BranchInfo->{BranchRevisions};

            if (@$BranchRevisions > 1 && $BranchRevisions->[0]->{Number} >
                ($BranchInfo->{MergeUpRevisions}->[0]->[1] || 0))
            {
                die "WARNING: Latest modifications in '$BranchPath'\n" .
                    "were not merged into '$BranchInfo->{UpstreamPath}'.\n" .
                    "Use --force to remove the branch anyway.\n"
            }
        }

        for (@{$BranchInfo->{BranchDirs}})
        {
            MarkPath("$BranchPath/$_", \%ModTree, 'rm')
        }
    }

    my $ExistingStructure =
        $Self->GetTreeContainingSubtree($RootURL, \%ModTree);

    while (my ($Subdir, $Subtree) = each %ModTree)
    {
        next if $Subdir eq '/';

        GetPathsToRemove(\@PathsToRemove, $ExistingStructure,
            $Subdir, $Subtree, $Subdir)
    }

    unless ($Action eq 'remove')
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
        my ($VerbING, $VerbED) = $Action eq 'create' ?
            qw(Creating Created) : $Action eq 'alter' ?
            qw(Updating Modified) : qw(Removing Removed);

        print STDERR "$VerbING branch '$BranchPath'...\n";

        $Self->{SVN}->RunMUCC('--root-url', $RootURL,
            '--message', "$VerbED branch '$BranchPath'.", @MUCCCommands);

        unless ($Action eq 'remove' or $Action eq 'create')
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
    my ($Self, $RootURL, $BranchPath, $UpstreamPath, @BranchDirs) = @_;

    $Self->ShapeBranch('create', undef, $RootURL,
        $BranchPath, $UpstreamPath, @BranchDirs)
}

sub Alter
{
    my ($Self, $Force, $RootURL, $BranchPath, @BranchDirs) = @_;

    $Self->ShapeBranch('alter', $Force, $RootURL,
        $BranchPath, undef, @BranchDirs)
}

sub Remove
{
    my ($Self, $Force, $RootURL, $BranchPath) = @_;

    $Self->ShapeBranch('remove', $Force, $RootURL, $BranchPath)
}

sub SetRawMergeProp
{
    my ($Self, $BranchPath, $Changes, @BranchDirs) = @_;

    $Self->{SVN}->RunSubversion('propset', '-R', 'ncbi:raw',
        "Please run \"$Self->{MyName} commit_merge " .
        NCBI::SVN::Branching::Util::SimplifyBranchPath($BranchPath) .
        "\" to merge $Changes", @BranchDirs)
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $SourceRev) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo;

    if ($Direction eq 'up')
    {
        $BranchInfo = NCBI::SVN::Branching::BranchAndUpstreamInfo->new($RootURL,
            $BranchPath, $SourceRev, undef);

        $SourceRev = $BranchInfo->{BranchRevisions}->[0]->{Number}
    }
    else
    {
        $BranchInfo = NCBI::SVN::Branching::BranchAndUpstreamInfo->new($RootURL,
            $BranchPath, undef, $SourceRev);

        my $UpstreamRevisions = $BranchInfo->{UpstreamRevisions};

        $SourceRev = @$UpstreamRevisions ? $UpstreamRevisions->[0]->{Number} :
            $BranchInfo->{BranchRevisions}->[-1]->{Number} - 1;
    }

    my @BranchDirs = @{$BranchInfo->{BranchDirs}};

    print STDERR "Running svn status on branch directories...\n";

    for ($Self->{SVN}->ReadSubversionLines('status', @BranchDirs))
    {
        die "$Self->{MyName}: local modifications detected.\n"
            unless m/^(?:[\?~X]|    S)/o
    }

    print STDERR "Performing updates...\n";

    $Self->{SVN}->RunSubversion('update', @BranchDirs);

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
        print STDERR "Nothing to do.\n";
        return
    }

    print STDERR "Merging with r$SourceRev...\n";

    my $BaseURL = $RootURL . '/' . ($Direction eq 'up' ?
        $BranchPath : $UpstreamPath) . '/';

    for my $LocalDir (@BranchDirs)
    {
        for my $RevRange (@MergePlan)
        {
            print STDERR "  $RevRange => $LocalDir\n";
            $Self->{SVN}->RunSubversion('merge', $RevRange,
                $BaseURL . $LocalDir, $LocalDir)
        }
    }

    $Self->SetRawMergeProp($BranchPath, "changes up to r$SourceRev from " .
        "'$SourcePath' into '$TargetPath'.", @BranchDirs)
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

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my @BranchDirs = @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
        $BranchPath)->{BranchDirs}};

    my $Message;

    for my $Dir (@BranchDirs)
    {
        my $PropValue =
            $Self->{SVN}->ReadSubversionStream('propget', 'ncbi:raw', $Dir);

        unless ($Message)
        {
            $Message = $PropValue
        }
        elsif ($Message ne $PropValue)
        {
            die "$Self->{MyName}: wrong cwd for the commit_merge operation.\n"
        }
    }

    my ($Changes) = $Message =~ m/to merge (changes .*?)[\r\n]*$/o;

    die "$Self->{MyName}: cannot retrieve log message.\n" unless $Changes;

    $Self->{SVN}->RunSubversion('propdel', '-R', 'ncbi:raw', @BranchDirs);

    eval
    {
        $Self->{SVN}->RunSubversion('commit',
            '-m', "Merged $Changes", @BranchDirs)
    };

    $Self->SetRawMergeProp($BranchPath, $Changes, @BranchDirs) if $@
}

sub MergeDiff
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('diff',
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchDirs}});

    my $Line;
    my $State = 0;
    my $Buffer;

    while (defined($Line = $Stream->ReadLine()))
    {
        if ($State == 0)
        {
            if ($Line =~ m/^[\r\n]*$/o)
            {
                $Buffer = $Line;
                $State = 1;
                next
            }
        }
        elsif ($State == 1)
        {
            if ($Line =~ m/^Property changes on: /o)
            {
                $Buffer .= $Line;
                $Line = $Stream->ReadLine();
                if ($Line =~ m/^_{66}/o)
                {
                    $Buffer .= $Line;
                    $State = 2;
                    next
                }
            }
        }
        elsif ($Line =~ m/^Name: ncbi:raw/o)
        {
            $Stream->ReadLine();
            next
        }
        elsif ($Line =~ m/^[\r\n]*$/o)
        {
            $State = 0;
            next
        }
        else
        {
            print $Buffer;
            $State = 0
        }

        print $Line
    }
}

sub MergeStat
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('status',
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchDirs}});

    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        next if $Line =~ s/^(.)M/$1 /o && $Line =~ m/^ {7}/o;

        print $Line
    }

    print "\nNOTE:  all property changes were omitted; " .
        "status may be inaccurate\n"
}

sub Svn
{
    my ($Self, $BranchPath, @CommandAndArgs) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    exec($Self->{SVN}->GetSvnPathname(), @CommandAndArgs,
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchDirs}})
}

sub DoSwitchUnswitch
{
    my ($Self, $BranchPath, $DoSwitch) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

    print STDERR ($DoSwitch ? 'Switching to' : 'Unswitching from') .
        " branch '$BranchPath'...\n";

    my $BaseURL = $RootURL . '/' . ($DoSwitch ?
        $BranchPath : $BranchInfo->{UpstreamPath}) . '/';

    for (@{$BranchInfo->{BranchDirs}})
    {
        $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
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

sub Update
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

    print STDERR "Updating working copy directories of '$BranchPath'...\n";

    my ($BranchDirs, $UpstreamPath, $ObsoleteBranchPaths) =
        @$BranchInfo{qw(BranchDirs UpstreamPath ObsoleteBranchPaths)};

    my @AffectedPaths = @$BranchDirs;

    push @AffectedPaths, @$ObsoleteBranchPaths if $ObsoleteBranchPaths;

    my $AffectedPathInfo = $Self->{SVN}->ReadInfo(@AffectedPaths);

    my $SwitchedToBranch;

    my @UpdateNonExisting;

    my @RecursiveUpdate;
    my @PathsToSwtich;

    for my $Path (@$BranchDirs)
    {
        my $Info = $AffectedPathInfo->{$Path};

        unless (-e $Path || !$Info)
        {
            push @UpdateNonExisting, $Path
        }
        elsif ($SwitchedToBranch)
        {
            if ($Info->{Path} eq "$BranchPath/$Path")
            {
                push @RecursiveUpdate, $Path
            }
            elsif ($Info->{Path} eq "$UpstreamPath/$Path")
            {
                push @PathsToSwtich, $Path
            }
            else
            {
                die "$Self->{MyName}: inconsistent working copy ($Path)\n";
            }
        }
        else
        {
            if ($Info->{Path} eq "$UpstreamPath/$Path")
            {
                push @RecursiveUpdate, $Path
            }
            elsif ($Info->{Path} eq "$BranchPath/$Path")
            {
                $SwitchedToBranch = 1;
                @PathsToSwtich = @RecursiveUpdate;
                @RecursiveUpdate = ($Path)
            }
            else
            {
                die "$Self->{MyName}: inconsistent working copy ($Path)\n";
            }
        }
    }

    if (@UpdateNonExisting)
    {
        my %NonExistingTree;

        for my $NonExistingPath (@UpdateNonExisting)
        {
            my @PathComponents = split('/', $NonExistingPath);

            if ($SwitchedToBranch)
            {
                push @PathsToSwtich, $NonExistingPath
            }
            else
            {
                push @RecursiveUpdate, $NonExistingPath;
                pop @PathComponents
            }

            my $Path = '.';
            my $PathExists = 1;
            my $Tree = \%NonExistingTree;

            for my $PathComponent (@PathComponents)
            {
                $Tree = ($Tree->{$PathComponent} ||= {});

                $Tree->{'/'} = 1
                    unless $PathExists &&= -e ($Path .= '/' . $PathComponent)
            }
        }

        @UpdateNonExisting = sort @{FindPathsInTree(\%NonExistingTree)};

        $Self->{SVN}->RunSubversion('update', '-N', @UpdateNonExisting)
            if @UpdateNonExisting
    }

    if (@PathsToSwtich)
    {
        my $RootURL = $Self->{SVN}->GetRootURL() ||
            die "$Self->{MyName}: not in a working copy\n";

        my $BaseURL = "$RootURL/$BranchPath/";

        for (@PathsToSwtich)
        {
            $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
        }
    }

    $Self->{SVN}->RunSubversion('update', @RecursiveUpdate) if @RecursiveUpdate
}

1
