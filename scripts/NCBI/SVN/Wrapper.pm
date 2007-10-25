# $Id$

use strict;
use warnings;

package NCBI::SVN::Wrapper::Stream;

use IPC::Open3;
use File::Temp qw/tempfile/;
use File::Spec;

sub new
{
    my ($Class, $SVN) = @_;

    return bless {SVN => $SVN}, $Class
}

sub Run
{
    my ($Self, @Args) = @_;

    my $WriteFH;

    $Self->{PID} = open3($WriteFH, $Self->{ReadFH},
        $Self->{ErrorFH} = tempfile(),
        $Self->{SVN}->GetSvnPathname(), '--non-interactive', @Args);

    close $WriteFH
}

sub ReadLine
{
    return readline $_[0]->{ReadFH}
}

sub Close
{
    my ($Self) = @_;

    return unless exists $Self->{PID};

    local $/ = undef;
    my $ErrorText = readline $Self->{ErrorFH};

    close($Self->{ReadFH});
    close($Self->{ErrorFH});

    waitpid(delete $Self->{PID}, 0);

    die $ErrorText if $?
}

package NCBI::SVN::Wrapper;

use Carp qw(confess);

sub FindProgram
{
    my ($Path, @Names) = @_;

    for my $Program (@Names)
    {
        my $Pathname = File::Spec->catfile($Path, $Program);

        return $Pathname if -f $Pathname
    }

    return undef
}

sub FindSubversion
{
    my ($Self) = @_;

    my ($SvnMUCCName, $MUCCName, @SvnNames) =
        !$Self->{Windows} ? qw(svnmucc mucc svn) :
            qw(svnmucc.exe mucc.exe svn.bat svn.exe);

    my ($SvnPathname, $SvnMUCCPathname, $MUCCPathname);

    for my $Path (File::Spec->path())
    {
        $SvnPathname ||= FindProgram($Path, @SvnNames);
        $SvnMUCCPathname ||= FindProgram($Path, $SvnMUCCName);
        $MUCCPathname ||= FindProgram($Path, $MUCCName);

        last if $SvnMUCCPathname && $SvnPathname
    }

    confess('Unable to find "svn" in PATH') unless $SvnPathname;

    @$Self{qw(SvnPathname MUCCPathname)} =
        ($SvnPathname, ($SvnMUCCPathname || $MUCCPathname))
}

sub new
{
    my ($Class, @Params) = @_;

    my $Self = bless {@Params}, $Class;

    $Self->{Windows} = 1 if $^O eq 'MSWin32' || $^O eq 'cygwin';

    $Self->FindSubversion() unless $Self->{SvnPathname};

    return $Self
}

sub SetSvnPathname
{
    my ($Self, $Pathname) = @_;

    $Self->{SvnPathname} = $Pathname
}

sub GetSvnPathname
{
    my ($Self) = @_;

    return $Self->{SvnPathname}
}

sub SetMUCCPathname
{
    my ($Self, $Pathname) = @_;

    $Self->{MUCCPathname} = $Pathname
}

sub GetMUCCPathname
{
    my ($Self) = @_;

    return $Self->{MUCCPathname}
}

sub GetRootURL
{
    my ($Self, $Path) = @_;

    $Path ||= '.';

    return -d '.svn' ? $Self->ReadInfo($Path)->{$Path}->{Root} : undef
}

sub RunOrDie
{
    my ($Self, @CommandAndParams) = @_;

    if (system(@CommandAndParams) != 0)
    {
        my $CommandLine = join(' ', @CommandAndParams);

        die "$Self->{MyName}: " .
            ($? == -1 ? "failed to execute $CommandLine\: $!" :
            $? & 127 ? "'$CommandLine' died with signal " . ($? & 127) :
            "'$CommandLine' exited with status " . ($? >> 8)) . "\n"
    }
}

sub RunSubversion
{
    my ($Self, @Params) = @_;

    $Self->RunOrDie($Self->GetSvnPathname(), @Params)
}

sub RunMUCC
{
    my ($Self, @Params) = @_;

    $Self->RunOrDie(($Self->GetMUCCPathname() or
        confess('Unable to find "svnmucc" or "mucc" in PATH')), @Params)
}

sub Run
{
    my ($Self, @Args) = @_;

    my $Stream = NCBI::SVN::Wrapper::Stream->new($Self);

    $Stream->Run(@Args);

    return $Stream
}

sub ReadFile
{
    my ($Self, $URL) = @_;

    return $Self->ReadSubversionStream('cat', $URL)
}

sub ReadFileLines
{
    my ($Self, $URL) = @_;

    return $Self->ReadSubversionLines('cat', $URL)
}

sub ReadInfo
{
    my ($Self, @Dirs) = @_;

    my $Stream = $Self->Run('info', @Dirs);

    my %Info;
    my $Path;
    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        $Line =~ s/[\r\n]+$//so;

        if ($Line =~ m/^Path: (.*?)$/os)
        {
            $Path = $1;
            $Path =~ s/\\/\//gso if $Self->{Windows}
        }
        elsif ($Line =~ m/^URL: (.*?)$/os)
        {
            $Info{$Path}->{Path} = $1
        }
        elsif ($Line =~ m/^Repository Root: (.*?)$/os)
        {
            $Info{$Path}->{Root} = $1
        }
    }

    my $Root;

    for my $DirInfo (values %Info)
    {
        ($Root, $Path) = @$DirInfo{qw(Root Path)};


        substr($Path, 0, length($Root), '') eq $Root or die;

        $DirInfo->{Path} = length($Path) > 0 ?
            substr($Path, 0, 1, '') eq '/' ? $Path : die : '.'
    }

    $Stream->Close();

    return \%Info
}

sub LogParsingError
{
    my ($Stream, $CurrentRevision, $State, $Line) = @_;

    local $/ = undef;
    $Stream->ReadLine();
    $Stream->Close();

    confess "svn log parsing error: r$CurrentRevision->{Number}; " .
        "state: $State; line '$Line'\n"
}

sub ReadLog
{
    my ($Self, @LogParams) = @_;

    my $Stream = $Self->Run('log', '--verbose', @LogParams);

    my $Line;
    my $State = 'initial';

    my @Revisions;
    my $CurrentRevision;
    my $NumberOfLogLines;

    while (defined($Line = $Stream->ReadLine()))
    {
        $Line =~ s/[\r\n]+$//;

        if ($State eq 'changed_path')
        {
            if ($Line)
            {
                $Line =~ m/^   ([AMDR]) (.+?)(?: \(from (.+):(\d+)\))?$/o or
                    LogParsingError($Stream, $CurrentRevision, $State, $Line);

                push @{$CurrentRevision->{ChangedPaths}}, [$1, $2, $3, $4]
            }
            else
            {
                $State = 'log_message';
            }
        }
        elsif ($State eq 'log_message')
        {
            $State = 'initial' if --$NumberOfLogLines == 0;
            $CurrentRevision->{LogMessage} .= $Line . "\n"
        }
        elsif ($State eq 'revision_header')
        {
            my %NewRev = (LogMessage => '', ChangedPaths => []);

            push @Revisions, ($CurrentRevision = \%NewRev);

            (@NewRev{qw(Number Author)}, $NumberOfLogLines) = $Line =~
                m/^r(\d+) \| (.+?) \| .+? \| (\d+) lines?$/o or
                    LogParsingError($Stream, $CurrentRevision, $State, $Line);

            $State = $NumberOfLogLines > 0 ? 'changed_path_header' : 'initial'
        }
        elsif ($State eq 'changed_path_header')
        {
            LogParsingError($Stream, $CurrentRevision, $State, $Line)
                if $Line ne 'Changed paths:';

            $State = 'changed_path'
        }
        elsif ($State eq 'initial')
        {
            LogParsingError($Stream, $CurrentRevision, $State, $Line)
                unless $Line =~ m/^-{70}/o;

            $State = 'revision_header'
        }
    }

    $Stream->Close();

    $_->{LogMessage} =~ s/[\r\n]+$//so for @Revisions;

    return \@Revisions;
}

sub GetLatestRevision
{
    my ($Self, $URL) = @_;

    my $Stream = $Self->Run('info', $URL);

    my $Line;
    my $Revision;

    while (defined($Line = $Stream->ReadLine()))
    {
        if ($Line =~ m/^Revision: (\d+)/os)
        {
            $Revision = $1;
            last
        }
    }

    local $/ = undef;
    $Stream->ReadLine();
    $Stream->Close();

    return $Revision
}

sub ReadSubversionStream
{
    my ($Self, @CommandAndParams) = @_;

    my $Stream = $Self->Run(@CommandAndParams);

    local($/) = undef;

    my $Contents = $Stream->ReadLine();

    $Stream->Close();

    return $Contents
}

sub ReadSubversionLines
{
    my ($Self, @CommandAndParams) = @_;

    my $Stream = $Self->Run(@CommandAndParams);

    my @Lines;
    my $Line;

    while (defined($Line = $Stream->ReadLine()))
    {
        $Line =~ s/[\r\n]+$//so;
        push @Lines, $Line
    }

    $Stream->Close();

    return @Lines
}

1
