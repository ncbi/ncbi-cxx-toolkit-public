# $Id$

package NCBI::SVN::Branching;

use strict;
use warnings;

use base qw(NCBI::SVN::Base);

use File::Temp qw/tempfile/;

use NCBI::SVN::Branching::Util;
use NCBI::SVN::Branching::BranchInfo;
use NCBI::SVN::Branching::WorkingCopyInfo;
use NCBI::SVN::Wrapper;

sub List
{
    my ($Self, $RootURL) = @_;

    print "Managed branches in $RootURL\:\n";

    print NCBI::SVN::Branching::Util::SimplifyBranchPath($_) . "\n"
        for $Self->{SVN}->ReadFileLines($RootURL . '/branches/branch_list')
}

sub PrintMergeRevisionHistory
{
    my ($Self, $Direction, $SourcePath, $TargetPath, $Revisions) = @_;

    sub Times
    {
        return @{$_[0]} ? @{$_[0]} > 1 ? @{$_[0]} . ' times' : 'once' : 'never'
    }

    print "Merged [$Direction] from '$SourcePath' into '$TargetPath': " .
        Times($Revisions) . "\n";

    for my $Revision (@$Revisions)
    {
        my $Line = "  r$Revision->{Number} (from " .
            "r$Revision->{SourceRevisionNumber})";

        if (my $Description = $Revision->{MergeDescription})
        {
            $Line .= "\t- ";
            $Description =~ s/\s+$//so;
            $Description =~ s/^\s+//so;
            $Description =~ s/\s+/ /gso;
            $Description = "$1..." if $Description =~ m/^(.{37}).{4}/;
            $Line .= $Description
        }

        print $Line . "\n"
    }
}

sub Info
{
    my ($Self, $RootURL, $BranchPath) = @_;

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);
    my $UpstreamInfo = NCBI::SVN::Branching::UpstreamInfo->new($BranchInfo);

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    print "Branch path: $BranchPath\n" .
        "Created: in r$BranchInfo->{BranchCreationRevision}->{Number}" .
        " from $UpstreamPath\@$BranchInfo->{BranchSourceRevision}\n";

    print "Branch structure:\n";
    for my $Path (@{$BranchInfo->{BranchPaths}})
    {
        print "  $Path\n"
    }

    $Self->PrintMergeRevisionHistory('up', $BranchPath, $UpstreamPath,
        $UpstreamInfo->{MergeUpRevisions});

    $Self->PrintMergeRevisionHistory('down', $UpstreamPath, $BranchPath,
        $BranchInfo->{MergeDownRevisions})
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
    my ($Self, $Action, $Force, $RootURL, $BranchPath,
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
    my ($Self, $RootURL, $BranchPath,
        $UpstreamPath, $SourceRev, @BranchPaths) = @_;

    $Self->ShapeBranch('create', undef, $RootURL,
        $BranchPath, $UpstreamPath, $SourceRev, @BranchPaths)
}

sub Alter
{
    my ($Self, $Force, $RootURL, $BranchPath, @BranchPaths) = @_;

    $Self->ShapeBranch('alter', $Force, $RootURL,
        $BranchPath, undef, undef, @BranchPaths)
}

sub Grow
{
    my ($Self, $RootURL, $BranchPath, $SourceRev, @BranchPaths) = @_;

    $Self->ShapeBranch('grow', undef, $RootURL,
        $BranchPath, undef, $SourceRev, @BranchPaths)
}

sub Truncate
{
    my ($Self, $RootURL, $BranchPath, @BranchPaths) = @_;

    $Self->ShapeBranch('truncate', undef, $RootURL,
        $BranchPath, undef, undef, @BranchPaths)
}

sub Remove
{
    my ($Self, $Force, $RootURL, $BranchPath) = @_;

    $Self->ShapeBranch('remove', $Force, $RootURL, $BranchPath)
}

sub SetRawMergeProp
{
    my ($Self, $BranchPath, $Changes, @BranchPaths) = @_;

    $Self->{SVN}->RunSubversion('propset', '-R', 'ncbi:raw',
        "Please run '$Self->{MyName} commit_merge " .
        NCBI::SVN::Branching::Util::SimplifyBranchPath($BranchPath) .
        "' to merge $Changes", @BranchPaths)
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $SourceRev) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);
    my $UpstreamInfo = NCBI::SVN::Branching::UpstreamInfo->new($BranchInfo);

    my @BranchPaths = @{$BranchInfo->{BranchPaths}};

    print STDERR "Running svn status on branch directories...\n";

    for ($Self->{SVN}->ReadSubversionLines('status', @BranchPaths))
    {
        die "$Self->{MyName}: local modifications detected.\n"
            unless m/^(?:[\?~X]|    S)/o
    }

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    my ($SourcePath, $TargetPath,
        $LastSourceRev, $LastTargetRev, $SourceRevisions) =
        $Direction eq 'up' ?
            ($BranchPath, $UpstreamPath,
                $UpstreamInfo->{LastUpSyncRevisionNumber},
                $BranchInfo->{LastDownSyncRevisionNumber},
                $BranchInfo->{BranchRevisions}) :
            ($UpstreamPath, $BranchPath,
                $BranchInfo->{LastDownSyncRevisionNumber},
                $UpstreamInfo->{LastUpSyncRevisionNumber},
                $UpstreamInfo->{UpstreamRevisions});

    $SourceRev ||= $SourceRevisions->[0]->{Number} || 0;

    unless ($SourceRev > $LastSourceRev)
    {
        die "$Self->{MyName}: '$TargetPath' is already " .
            "in sync with r$LastSourceRev of '$SourcePath'\n"
    }

    my $SourceURL = $RootURL . '/' . $SourcePath . '/';
    my $TargetURL = $RootURL . '/' . $TargetPath . '/';

    print STDERR "Performing updates...\n";

    $Self->{SVN}->RunSubversion('update', @BranchPaths);

    print STDERR "Merging with r$SourceRev...\n";

    my $LastMergeRevision = $UpstreamInfo->{MergeRevisions}->[0];

    my $BranchPathInfo = $BranchInfo->{BranchPathInfo};

    if ($LastMergeRevision and
        $LastMergeRevision->{MergeDirection} ne $Direction)
    {
        # Previous merge was of the opposite direction.
        for my $Path (@BranchPaths)
        {
            my $PathCreationRevisionNumber =
                $BranchPathInfo->{$Path}->{PathCreationRevision}->{Number};

            my $LocalLastTargetRev = $LastTargetRev;
            my $LocalSourceRev = $SourceRev;

            if ($Direction eq 'up')
            {
                $LocalSourceRev = $PathCreationRevisionNumber
                    if $LocalSourceRev < $PathCreationRevisionNumber
            }
            else
            {
                $LocalLastTargetRev = $PathCreationRevisionNumber
                    if $LocalLastTargetRev < $PathCreationRevisionNumber
            }

            $Self->{SVN}->RunSubversion('merge',
                $TargetURL . $Path . '@' . $LocalLastTargetRev,
                $SourceURL . $Path . '@' . $LocalSourceRev,
                $Path)
        }
    }
    else
    {
        # Either there were no merge revisions or
        # previous merge was of the same direction.
        for my $Path (@BranchPaths)
        {
            my $PathCreationRevisionNumber =
                $BranchPathInfo->{$Path}->{PathCreationRevision}->{Number};

            my $LocalLastSourceRev = $LastSourceRev;

            if ($Direction eq 'up')
            {
                $LocalLastSourceRev = $PathCreationRevisionNumber
                    if $LocalLastSourceRev < $PathCreationRevisionNumber
            }

            if ($LocalLastSourceRev < $SourceRev)
            {
                $Self->{SVN}->RunSubversion('merge',
                    "-r$LocalLastSourceRev\:$SourceRev",
                        $SourceURL . $Path, $Path)
            }
        }
    }

    $Self->SetRawMergeProp($BranchPath, "changes up to r$SourceRev from " .
        "'$SourcePath' into '$TargetPath'.", @BranchPaths)
}

sub MergeDownInto
{
    my $Self = shift;

    return $Self->DoMerge('down', @_)
}

sub MergeUpFrom
{
    my $Self = shift;

    return $Self->DoMerge('up', @_)
}

sub CommitMerge
{
    my ($Self, $BranchPath, $LogMessage) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo =
        NCBI::SVN::Branching::BranchInfo->new($RootURL, $BranchPath);

    my @BranchPaths = @{$BranchInfo->{BranchPaths}};

    my $Message;

    for my $Path (@BranchPaths)
    {
        my $PropValue =
            $Self->{SVN}->ReadSubversionStream('propget', 'ncbi:raw', $Path);

        unless ($Message)
        {
            $Message = $PropValue
        }
        elsif ($Message ne $PropValue)
        {
            die "$Self->{MyName}: wrong cwd for the commit_merge operation.\n"
        }
    }

    my ($Changes, $SourcePath, $TargetPath) = $Message =~
        m/(changes up to r\d+ from '(.+?)' into '(.+?)'\.)/so or
            die "$Self->{MyName}: cannot retrieve log message.\n";

    my $UpstreamPath = $BranchInfo->{UpstreamPath};

    if ($SourcePath eq $BranchPath)
    {
        die "$Self->{MyName}: wrong BRANCH_PATH argument for this merge.\n"
            if $TargetPath ne $UpstreamPath;

        die "$Self->{MyName}: upstream merge requires a log message.\n"
            unless $LogMessage
    }
    elsif ($TargetPath ne $BranchPath || $SourcePath ne $UpstreamPath)
    {
        die "$Self->{MyName}: wrong BRANCH_PATH argument for this merge.\n"
    }

    $Self->{SVN}->RunSubversion('propdel', '-R', 'ncbi:raw', @BranchPaths);

    eval
    {
        $Self->{SVN}->RunSubversion('commit', '-m', 'Merged ' . ($LogMessage ?
            $Changes . "\n\n" . $LogMessage : $Changes), @BranchPaths)
    };

    $Self->SetRawMergeProp($BranchPath, $Changes, @BranchPaths) if $@
}

sub MergeDiff
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('diff',
        @{NCBI::SVN::Branching::BranchInfo->new($RootURL,
            $BranchPath)->{BranchPaths}});

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
            $BranchPath)->{BranchPaths}});

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
            $BranchPath)->{BranchPaths}})
}

sub DoUpdateAndSwitch
{
    my ($Self, $WorkingCopyInfo, $TargetPath,
        $PathsToUpdate, $PathsToSwtich) = @_;

    my ($RootURL, $MissingTree) = @$WorkingCopyInfo{qw(RootURL MissingTree)};

    $Self->{SVN}->RunSubversion('update', '-N', @$MissingTree) if @$MissingTree;

    $Self->{SVN}->RunSubversion('update', @$PathsToUpdate) if @$PathsToUpdate;

    my $BaseURL = "$RootURL/$TargetPath/";

    for (@$PathsToSwtich,
        @{$WorkingCopyInfo->{IncorrectlySwitched}},
        @{$WorkingCopyInfo->{MissingBranchPaths}})
    {
        $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
    }

    $BaseURL = "$RootURL/$WorkingCopyInfo->{WorkingDirPath}/";

    for (@{$WorkingCopyInfo->{ObsoletePaths}})
    {
        $Self->{SVN}->RunSubversion('switch', $BaseURL . $_, $_)
    }
}

sub Switch
{
    my ($Self, $BranchPath, $Parent) = @_;

    my $WorkingCopyInfo =
        NCBI::SVN::Branching::WorkingCopyInfo->new($BranchPath);

    my $TargetPath = $Parent ?
        $WorkingCopyInfo->{BranchInfo}->{UpstreamPath} : $BranchPath;

    print STDERR "Switching to '$TargetPath'...\n";

    $Self->DoUpdateAndSwitch($WorkingCopyInfo, $TargetPath,
        @$WorkingCopyInfo{$Parent ? qw(SwitchedToParent SwitchedToBranch) :
            qw(SwitchedToBranch SwitchedToParent)})
}

sub Update
{
    my ($Self, $BranchPath) = @_;

    my $WorkingCopyInfo =
        NCBI::SVN::Branching::WorkingCopyInfo->new($BranchPath);

    my ($SwitchedToBranch, $SwitchedToParent) =
        @$WorkingCopyInfo{qw(SwitchedToBranch SwitchedToParent)};

    my $TargetPath;
    my $PathsToUpdate;
    my $PathsToSwtich;

    if (@$SwitchedToBranch)
    {
        $TargetPath = $BranchPath;
        $PathsToUpdate = $SwitchedToBranch;
        $PathsToSwtich = $SwitchedToParent
    }
    elsif (@$SwitchedToParent)
    {
        $TargetPath = $WorkingCopyInfo->{BranchInfo}->{UpstreamPath};
        $PathsToUpdate = $SwitchedToParent;
        $PathsToSwtich = []
    }
    else
    {
        die "$Self->{MyName}: unable to find a check-out of '$BranchPath'.\n"
    }

    print STDERR "Updating working copy directories of '$BranchPath'...\n";

    $Self->DoUpdateAndSwitch($WorkingCopyInfo, $TargetPath,
        $PathsToUpdate, $PathsToSwtich)
}

1
