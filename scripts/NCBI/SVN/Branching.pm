# $Id$

use strict;
use warnings;

package NCBI::SVN::Branching::Util;

sub FindTreeNode
{
    my ($Tree, $Path) = @_;

    for my $Subdir (split('/', $Path))
    {
        $Tree = ($Tree->{$Subdir} ||= {})
    }

    return $Tree
}

package NCBI::SVN::BranchInfo;

use base qw(NCBI::SVN::Base);

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
    my ($Self, $SourcePath, $BranchDir, $SourceRevision, $Revision) = @_;

    my $UpstreamPath = $SourcePath;

    length($UpstreamPath) > length($BranchDir) and
        substr($UpstreamPath, -length($BranchDir),
            length($BranchDir), '') eq $BranchDir
        or die 'branch structure does not replicate ' .
            "upstream structure in r$Revision->{Number}";

    unless (my $SameUpstreamPath = $Self->{UpstreamPath})
    {
        $Self->{UpstreamPath} = $UpstreamPath
    }
    else
    {
        die "inconsistent source paths in r$Revision->{Number}"
            if $UpstreamPath ne $SameUpstreamPath
    }

    unless (my $SingleSourceRevision = $Self->{LastSynchRevision})
    {
        $Self->{LastSynchRevision} = $SourceRevision
    }
    else
    {
        die "inconsistent source revision numbers in r$Revision->{Number}"
            if $SourceRevision != $SingleSourceRevision
    }
}

sub ModelBranchStructure
{
    my ($Self, $BranchStructure, $Revision, $CommonTarget) = @_;

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

                $Self->SetUpstreamAndSynchRev($SourcePath,
                    $BranchDir, $SourceRevision)
            }
        }
        elsif ($ChangeType eq 'A' && $SourcePath)
        {
            my $BranchDir = CutOffCommonTargetPrefix($TargetPath,
                $CommonTarget, $Revision);

            NCBI::SVN::Branching::Util::FindTreeNode($BranchStructure,
                $BranchDir)->{'/'} = 1;

            $Self->SetUpstreamAndSynchRev($SourcePath,
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

sub new
{
    my ($Class, $RootURL, $BranchPath, $MaxBranchRev) = @_;

    $MaxBranchRev ||= 'HEAD';

    my @BranchDirs;
    my @BranchRevisions;
    my @MergeDownRevisions;

    my $Self = $Class->SUPER::new(
        BranchPath => $BranchPath,
        BranchDirs => \@BranchDirs,
        BranchRevisions => \@BranchRevisions,
        MergeDownRevisions => \@MergeDownRevisions
    );

    print STDERR "Gathering information about $BranchPath...\n";

    my $RevisionLog = $Self->{SVN}->ReadLog('--stop-on-copy',
        "-r$MaxBranchRev\:1", $RootURL, $BranchPath);

    my %BranchStructure;
    my $CommonTarget = "/$BranchPath/";

    for my $Revision (@$RevisionLog)
    {
        push @BranchRevisions, $Revision;

        if ($Revision->{LogMessage} =~ m/^Created branch '(.+?)'.$/o &&
            $1 eq $BranchPath)
        {
            $Self->{BranchCreationRevision} = $Revision;

            $Self->ModelBranchStructure(\%BranchStructure,
                $Revision, $CommonTarget);

            last
        }
    }

    unless ($Self->{UpstreamPath} &&
        ($Self->{BranchSourceRevision} = $Self->{LastSynchRevision}))
    {
        die 'unable to determine branch source'
    }

    for my $Revision (reverse @BranchRevisions)
    {
        my $LogMessage = $Revision->{LogMessage};

        if ($LogMessage =~ m/^Modified branch '(.+?)'.$/o && $1 eq $BranchPath)
        {
            $Self->ModelBranchStructure(\%BranchStructure,
                $Revision, $CommonTarget)
        }
        elsif ($LogMessage =~
            m/^Merged changes up to r(\d+) from '.+' into '(.+)'.$/o &&
                $2 eq $BranchPath)
        {
            unshift @MergeDownRevisions,
                [$Revision, $Self->{LastSynchRevision} = $1]
        }
    }

    while (my ($Dir, $Contents) = each %BranchStructure)
    {
        ExtractBranchDirs(\@BranchDirs, $Dir, $Contents)
    }

    @BranchDirs = sort @BranchDirs;

    $Self->{UpstreamPath} =~ s/^\/?(.+?)\/?$/$1/;

    return $Self
}

package NCBI::SVN::BranchAndUpstreamInfo;

use base qw(NCBI::SVN::BranchInfo);

sub new
{
    my ($Class, $RootURL, $BranchPath, $MaxBranchRev, $MaxUpstreamRev) = @_;

    my $Self = $Class->SUPER::new($RootURL, $BranchPath, $MaxBranchRev);

    $MaxUpstreamRev ||= 'HEAD';

    my $UpstreamPath = $Self->{UpstreamPath};

    print STDERR "Gathering information about $UpstreamPath...\n";

    my $UpstreamRevisions = $Self->{SVN}->ReadLog('--stop-on-copy',
        "-r$MaxUpstreamRev\:$Self->{BranchRevisions}->[-1]->{Number}",
        $RootURL, map {"$UpstreamPath/$_"} @{$Self->{BranchDirs}});

    my @MergeUpRevisions;

    for my $Revision (@$UpstreamRevisions)
    {
        if ($Revision->{LogMessage} =~
            m/^Merged changes up to r(\d+) from '(.+)' into '.+'.$/o &&
                $2 eq $BranchPath)
        {
            push @MergeUpRevisions, [$Revision, $1]
        }
    }

    @$Self{qw(UpstreamRevisions MergeUpRevisions)} =
        ($UpstreamRevisions, \@MergeUpRevisions);

    return $Self
}

package NCBI::SVN::Branching;

use base qw(NCBI::SVN::Base);

use File::Temp qw/tempfile/;

use NCBI::SVN::Wrapper;

sub List
{
    my ($Self, $RootURL) = @_;

    print "Managed branches in $RootURL\:\n";

    for my $BranchPath ($Self->{SVN}->ReadFileLines($RootURL .
        '/branches/branch_list'))
    {
        $BranchPath =~ s/^branches\///o;
        print "$BranchPath\n"
    }
}

sub Info
{
    my ($Self, $RootURL, $BranchPath) = @_;

    my $BranchInfo = NCBI::SVN::BranchAndUpstreamInfo->new($RootURL, $BranchPath);

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

sub GetRmCommands
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
                if GetRmCommands(\@RmCommands, $DirExistingStructure,
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

sub GetMkdirCommands
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

        GetMkdirCommands($MkdirCommands,
            $ExistingStructure->{$Subdir}, $Subtree, "$Path/$Subdir")
    }
}

sub ShapeBranch
{
    my ($Self, $Action, $RootURL, $BranchPath, $UpstreamPath, @BranchDirs) = @_;

    my @RmCommands;
    my @MkdirCommands;
    my @CopyCommands;
    my @PutCommands;

    my $RewriteBranchList;
    my $BranchListFN;

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

    # If the current operation affects branch_list, add a MUCC
    # command to (re-)create branch_list.
    if ($RewriteBranchList)
    {
        my $BranchListFH;

        ($BranchListFH, $BranchListFN) = tempfile();

        print $BranchListFH "$_\n" for @Branches;

        close $BranchListFH;

        push @PutCommands, 'put', $BranchListFN, 'branches/branch_list'
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
            push @CopyCommands, 'cp', $SourceRevision, $SourcePath, $TargetPath
        }
    }
    elsif ($Action eq 'alter')
    {
        my $BranchInfo = NCBI::SVN::BranchInfo->new($RootURL, $BranchPath);

        $UpstreamPath = $BranchInfo->{UpstreamPath};

        my %OldBranchDirs = map {$_ => 1} @{$BranchInfo->{BranchDirs}};

        for my $Dir (@BranchDirs)
        {
            unless (delete $OldBranchDirs{$Dir})
            {
                my $SourcePath = "$UpstreamPath/$Dir";
                my $TargetPath = "$BranchPath/$Dir";

                MarkPath($TargetPath, \%ModTree, qw(rm mkparent));
                push @CopyCommands, 'cp', $BranchInfo->{LastSynchRevision},
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
        for (@{NCBI::SVN::BranchInfo->new($RootURL, $BranchPath)->{BranchDirs}})
        {
            MarkPath("$BranchPath/$_", \%ModTree, 'rm')
        }
    }

    my $ExistingStructure =
        $Self->GetTreeContainingSubtree($RootURL, \%ModTree);

    while (my ($Subdir, $Subtree) = each %ModTree)
    {
        next if $Subdir eq '/';

        GetRmCommands(\@RmCommands, $ExistingStructure,
            $Subdir, $Subtree, $Subdir)
    }

    unless ($Action eq 'remove')
    {
        while (my ($Subdir, $Subtree) = each %ModTree)
        {
            next if $Subdir eq '/';

            GetMkdirCommands(\@MkdirCommands,
                $ExistingStructure->{$Subdir}, $Subtree, $Subdir)
        }
    }

    my @Commands = (@RmCommands, @MkdirCommands, @CopyCommands, @PutCommands);

    # Unless there are no changes, (re)shape or remove the branch
    # with a single revision using MUCC.
    if (@Commands)
    {
        my ($VerbING, $VerbED) = $Action eq 'create' ?
            qw(Creating Created) : $Action eq 'alter' ?
            qw(Updating Modified) : qw(Removing Removed);

        print STDERR "$VerbING branch '$BranchPath'...\n";

        $Self->{SVN}->RunMUCC('--root-url', $RootURL,
            '--message', "$VerbED branch '$BranchPath'.", @Commands)
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

    $Self->ShapeBranch('create', $RootURL, $BranchPath, $UpstreamPath, @BranchDirs)
}

sub Alter
{
    my ($Self, $RootURL, $BranchPath, @BranchDirs) = @_;

    $Self->ShapeBranch('alter', $RootURL, $BranchPath, undef, @BranchDirs)
}

sub Remove
{
    my ($Self, $RootURL, $BranchPath) = @_;

    $Self->ShapeBranch('remove', $RootURL, $BranchPath)
}

sub SetRawMergeProp
{
    my ($Self, $Changes, @BranchDirs) = @_;

    $Self->{SVN}->RunSubversion('propset', '-R', 'ncbi:raw',
        qq(Please run "$Self->{MyName} commit_merge" to merge $Changes),
            @BranchDirs)
}

sub DoMerge
{
    my ($Self, $Direction, $BranchPath, $SourceRev) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo;

    if ($Direction eq 'up')
    {
        $BranchInfo = NCBI::SVN::BranchAndUpstreamInfo->new($RootURL,
            $BranchPath, $SourceRev, undef);

        $SourceRev = $BranchInfo->{BranchRevisions}->[0]->{Number}
    }
    else
    {
        $BranchInfo = NCBI::SVN::BranchAndUpstreamInfo->new($RootURL,
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

    $Self->SetRawMergeProp("changes up to r$SourceRev from " .
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

    my @BranchDirs = @{NCBI::SVN::BranchInfo->new($RootURL,
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

    $Self->SetRawMergeProp($Changes, @BranchDirs) if $@
}

sub MergeDiff
{
    my ($Self, $BranchPath) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $Stream = $Self->{SVN}->Run('diff',
        @{NCBI::SVN::BranchInfo->new($RootURL, $BranchPath)->{BranchDirs}});

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
        @{NCBI::SVN::BranchInfo->new($RootURL, $BranchPath)->{BranchDirs}});

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
        @{NCBI::SVN::BranchInfo->new($RootURL, $BranchPath)->{BranchDirs}})
}

sub DoSwitchUnswitch
{
    my ($Self, $BranchPath, $DoSwitch) = @_;

    my $RootURL = $Self->{SVN}->GetRootURL() ||
        die "$Self->{MyName}: not in a working copy\n";

    my $BranchInfo = NCBI::SVN::BranchInfo->new($RootURL, $BranchPath);

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

1
