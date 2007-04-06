#!/usr/bin/perl -w
# $Id$

use strict;

my ($ScriptDir, $ScriptName);

BEGIN
{
    ($ScriptDir, $ScriptName) = $0 =~ m/^(?:(.+)[\\\/])?(.+)$/;
    $ScriptDir ||= '.'
}

use lib $ScriptDir;

use NCBI::SVN::Update;
use NCBI::SVN::MultiSwitch;

use File::Spec;
use File::Basename;
use File::Find;
use Cwd;
use Fcntl qw(F_SETFD);

my $DefaultRepos = 'https://svn.ncbi.nlm.nih.gov/repos/toolkit';

my $Update = NCBI::SVN::Update->new(MyName => $ScriptName);

# Find (and cache) the path to Subversion before unsetting
# the PATH environment variable.
$Update->GetSvnPath();

my @UnsafeVars = qw(PATH IFS CDPATH ENV BASH_ENV TERM);
my %OldEnv;
@OldEnv{@UnsafeVars} = delete @ENV{@UnsafeVars};

$ScriptDir = Cwd::realpath($ScriptDir);

my @ProjectDirs = ("$ScriptDir/projects", "$ScriptDir/internal/projects");

sub Usage
{
    my ($Error) = @_;

    print STDERR $ScriptName . ' $Revision$' . <<EOF;

This script checks out files required for building the specified project
and optionally (re-)configures and builds it.

Usage:
    $ScriptName [-branches BranchConfFile] Project [BuildDir]

Where:
    Project - The name of the project you want to build or a pathname
        of the project listing file.

        If this argument looks like a project name (that is, it
        doesn't contain path information), then the '.lst' extension
        is added to it and the resulting file name is searched for
        in the following system directories:

EOF

    print STDERR "\t\t$_\n" for @ProjectDirs;

    print STDERR "\n\t\tAvailable project names are:\n\n";

    my @ProjectNames;
    find(sub {s/\.lst$// && push @ProjectNames, $_}, @ProjectDirs);

    my $Indent = "\t\t";
    my $MaxWidth = 80 - 2 * 8 - 6;

    my $Column = 0;

    for (@ProjectNames)
    {
        unless ($Column > 0)
        {
            while (length($_) > $MaxWidth)
            {
                m/^(.{$MaxWidth})(.+)$/;
                print STDERR "$Indent$1\n";
                $_ = $2
            }

            print STDERR $Indent
        }
        elsif (++$Column + length($_) <= $MaxWidth)
        {
            print STDERR ' '
        }
        else
        {
            if (length($_) > $MaxWidth)
            {
                if (($Column = $MaxWidth - $Column) > 0)
                {
                    m/^(.{$Column})(.+)$/;
                    print STDERR " $1";
                    $_ = $2
                }

                while (length($_) > $MaxWidth)
                {
                    m/^(.{$MaxWidth})(.+)$/;
                    print STDERR "\n$Indent$1";
                    $_ = $2
                }
            }

            print STDERR "\n$Indent";
            $Column = 0
        }

        print STDERR $_;

        if (($Column += length($_)) == $MaxWidth)
        {
            print STDERR "\n";
            $Column = 0
        }
    }

    print STDERR "\n" if $Column > 0;

    print STDERR <<EOF;

    BranchConfFile - Pathname of the file containing working copy directory
        switch plan. This parameter can only be used during the initial
        checkout - see the Description section below.

    BuildDir - Path to the target directory. The presence (or absence)
        of this argument designates the mode of operation - see
        the Description section below.

Description:
    This script supports two modes of operation:

    1. If you specify the BuildDir argument, the script will create the
        directory if necessary and check the specified portion of
        the C++ Toolkit tree out into it, along with any additional
        infrastructure needed for the build system to work.
        If the directory already exists, it must be empty.
        If the BranchConfFile file is specified, the script will switch
        working copy directories in accordance with this file.
        The script will then optionally configure and build
        the new tree.

    2. If you run this script from the top level of an existing working
        copy of the C++ source tree without sepcifying the BuildDir
        argument, it will update the sources and headers for the
        specified projects. The script will then optionally
        reconfigure and rebuild the tree.
        The -branches paramter cannot be used in this mode.

Examples:
    1. Perform initial checkout of project "connect" and its
        dependencies, then optionally configure and build them:

        \$ $ScriptName connect ./connect_build

    2. Update sources of the "connect" project and all of its
        dependencies to the latest versions from the repository
        and then optionally reconfigure and build them:

        \$ cd connect_build
        \$ $ScriptName connect

EOF

    if ($Error)
    {
        print STDERR "Error:  $Error.\n";
        exit 1
    }

    exit 0
}

my @Paths;

my %AllProjects;

my $RepositoryURL;

my @ProjectQueue;

# Load project listing files, which were either found in the project search
# paths by names of projects they described or were explicitly specified in
# the previously read project listing files using the #include directive.
sub ReadProjectListingFile
{
    my ($FileName, $Context) = @_;

    open FILE, '<', $FileName or die "$Context:$FileName: $!\n";

    while (<FILE>)
    {
        s/\s*$//so;
        s/\$$/\//so;

        $Context = "$FileName:$.";

        if (m/^\s*\&\s*(.*?)/)
        {
            push @ProjectQueue, $1, $Context
        }
        elsif (m/^\s*#\s*include\s+"(.*?)"/)
        {
            push @ProjectQueue, File::Spec->rel2abs($1,
                dirname($FileName)), $Context
        }
        elsif (m/!repo?s?\s+(.*)/)
        {
            $RepositoryURL = $1
        }
        elsif (m/^\.\/(.*)/)
        {
            push @Paths, $1
        }
        else
        {
            $AllProjects{$_} = 1;

            if (m{(?:corelib|dbapi/driver|objects/objmgr|objmgr|serial)/$}o)
            {
                push @Paths, 'include/' . $_ . 'impl'
            }

            push @Paths, 'include/' . $_, 'src/' . $_
        }
    }

    close FILE
}

sub FindProjectListing
{
    my ($Project, $Context) = @_;

    my ($Volume, $Dir, undef) = File::Spec->splitpath($Project);

    return $Project if (($Volume || $Dir) && -f $Project);

    # Search through the registered directories with
    # project listings.
    for my $ProjectDir (@ProjectDirs)
    {
        my $FileName = "$ProjectDir/$Project.lst";

        return $FileName if -f $FileName
    }

    die "$Context: unable to find project '$Project'\n"
}

Usage() if @ARGV < 1 || $ARGV[0] eq '--help';

my ($BranchConfFile, $MainProject, $BuildDir);

while (@ARGV)
{
    my $Arg = shift @ARGV;

    if ($Arg eq '-switch-map' || $Arg eq '-branches')
    {
        $BranchConfFile = shift @ARGV or Usage("Pathname missing after $Arg")
    }
    elsif (!$MainProject)
    {
        $MainProject = $Arg
    }
    elsif (!$BuildDir)
    {
        $BuildDir = $Arg
    }
    else
    {
        Usage('Too many command line arguments')
    }
}

Usage('Missing mandatory argument Project') unless $MainProject;

Usage('-branches can only be used in the checkout mode')
    if $BranchConfFile && !$BuildDir;

$MainProject = FindProjectListing($MainProject, $ScriptName);

ReadProjectListingFile($MainProject, $ScriptName);

my $NewCheckout;

if ($BuildDir)
{
    die "Error: $BuildDir is not empty.\n" if -d "$BuildDir/.svn";

    mkdir $BuildDir;

    my $ProjectListFile = "$BuildDir/projects";
    if (eval {symlink('', ''); 1})
    {
        unlink $ProjectListFile;
        symlink(File::Spec->file_name_is_absolute($MainProject) ?
            $MainProject : File::Spec->rel2abs($MainProject), $ProjectListFile)
            or die $!;
    }
    else
    {
        open IN, '<', $MainProject or die "$MainProject: $!\n";
        open OUT, '>', $ProjectListFile or die "$ProjectListFile: $!\n";
        print OUT while <IN>;
        close OUT;
        close IN
    }

    push @Paths, qw(./compilers ./scripts ./include/common);

    $RepositoryURL = $DefaultRepos;
    $NewCheckout = 1
}
else
{
    die "No directory specified and no existing checkout detected.\n"
        unless -d '.svn' && -e 'projects';

    $BuildDir = '.'
}

# For each project included by an ampersand reference.
while (@ProjectQueue)
{
    my $Project = shift @ProjectQueue;
    my $Context = shift @ProjectQueue;

    ReadProjectListingFile(FindProjectListing($Project, $Context), $Context)
}

if ($RepositoryURL)
{
    $Update->RunSubversion(($NewCheckout ? 'co' : 'switch'), '-N',
        "$RepositoryURL/trunk/c++", $BuildDir)
}

my $MultiSwitch;

$MultiSwitch = NCBI::SVN::MultiSwitch->new(MyName => $ScriptName,
    MapFileName => $BranchConfFile) if $BranchConfFile;

chdir $BuildDir;

$Update->UpdateDirList(@Paths);

$MultiSwitch->SwitchUsingMap() if $MultiSwitch;

exit 0 if $^O eq 'MSWin32';

my @ConfigOptions;

for my $OptProject (qw(dbapi serial objects app internal))
{
    push @ConfigOptions, ($AllProjects{$OptProject} ?
        '--with-' : '--without-') . $OptProject
}

while (my ($Var, $Val) = each %OldEnv)
{
    $ENV{$Var} = $Val if defined $Val
}

my ($Child, $Parent);

pipe $Child, $Parent or die $!;

my $PID = fork();

if ($PID)
{
    # parent
    close $Child;
    print $Parent <<'SHELL';
#!/bin/sh

# Author: Aaron Ucko, NCBI
#
#  Reconfigures just the specified projects.
#  Does NOT do anything with Windows or MacOS project files.

Usage() {
    cat <<EOF
This script configures and builds the C++ Toolkit source tree.
Usage: $0 <project-list> [<directory>]
EOF
}

Die() {
    echo "$@" >& 2
    exit 1
}

ConfirmYes() {
    while :; do
        read answer
        case "$answer" in
            [Yy]*) break ;;
            [Nn]*) exit 0 ;;
        esac
        echo "Please answer Y(es) or N(o)."
    done
}

if test "x$1" = "x--help"; then
    Usage
    exit 0
fi

tty -s || exit 0

n=0
for d in */build; do
    if test "x$d" = "x*/build"; then
    break # No real matches
    else
    n=`expr $n + 1`
    eval "dir$n=$d"
    fi
done
if [ $n -eq 0 ]; then
    # No builds; presumably a new checkout
    echo 'Would you like to configure this tree? (y/n)'
    ConfirmYes
    config_options=$*
    case "`uname -s`:`uname -m`" in
        SunOS:*)
            n=3
            conf1=./configure
            conf2="compilers/WorkShop.sh 32"
            conf3="compilers/WorkShop.sh 64"
            ;;
        Linux:i?86)
            n=3
            conf1=./configure
            conf2=compilers/ICC.sh
            conf3="compilers/KCC.sh 32"
            ;;
        IRIX*:*)
            n=3
            conf1=./configure
            conf2="compilers/MIPSpro73.sh 32"
            conf3="compilers/MIPSpro73.sh 64"
            ;;
        *:alpha)
            n=2
            conf1=./configure
            conf2=compilers/Compaq.sh
            ;;
        *)
            n=1
            configure=./configure
            ;;
    esac
    if [ $n -gt 1 ]; then
        echo ; echo "Which configure script would you like to use?"
        i=1
        while [ $i -le $n ]; do
            eval echo "$i - \$conf$i"
            i=`expr $i + 1`
        done
        while :; do
            read answer
            case "$answer" in
                [1-9])
                    if [ $answer -le $n ]; then
                        eval configure="\$conf$answer"
                        break
                    fi
                    ;;
            esac
            echo "Please give a number between 1 and $n."
        done
    fi
    case "$configure" in
        */ICC*)
            IA32ROOT="/opt/intel/compiler80"
            PATH="$IA32ROOT/bin:$PATH"
            LD_LIBRARY_PATH="$IA32ROOT/lib:$LD_LIBRARY_PATH"
            INTEL_FLEXLM_LICENSE="/opt/intel/licenses"
            export IA32ROOT PATH LD_LIBRARY_PATH INTEL_FLEXLM_LICENSE
            ;;
        */KCC*)
            PATH="/usr/kcc/KCC_BASE/bin:$PATH"
            export PATH
            ;;
    esac
    cat <<EOF
Current options to configure (deduced from project list):
$config_options
If you would like to pass any additional options, please enter them now;
otherwise, just hit return.
EOF
    read more_options
    config_options="$config_options $more_options"
    $run $configure $config_options
    echo ; echo "Would you like to compile this tree? (y/n)"
    ConfirmYes
    $run cd */build
    $run make all_p
else
    echo ; echo "Would you like to reconfigure an existing build of this tree?"
    echo "0 - No, thank you."
    i=1
    while [ $i -le $n ]; do
        eval echo "$i - Yes, please reconfigure \$dir$i."
        i=`expr $i + 1`
    done
    while :; do
        read answer
        case "$answer" in
            0) exit 0 ;;
            [1-9]|[1-9][0-9]) # Nobody is likely to have more than 99 configs!
                if [ $answer -le $n ]; then
                    eval $run cd \$dir$answer
                    $run chmod +x reconfigure.sh
                    $run ./reconfigure.sh reconf
                    break
                fi
                ;;
        esac
        echo "Please give a number between 0 and $n."
    done
    echo ; echo "Would you like to recompile this build? (y/n)"
    ConfirmYes
    $run make all_p
fi

echo ; echo "Would you like to run tests on this build? (y/n)"
ConfirmYes
$run make check_p
SHELL
    close $Parent;
    wait()
}
elsif (defined $PID)
{
    # child
    close $Parent;
    fcntl $Child, F_SETFD, 0;
    exec '/bin/sh', '/dev/fd/' . fileno($Child), @ConfigOptions or die $!
}
else
{
    die "fork() failed: $!"
}
