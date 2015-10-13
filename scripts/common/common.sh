#
#  $Id$
#  ATTENTION!  This is to be 'included' (using ". common.sh'), not executed!
#              Then, you execute the "COMMON_xxx" function(s) you like inside
#              your script.
#

COMMON_AddRunpath()
{
    x_add_rpath="$1"

    test -z "$x_add_rpath"  &&  return 0

    if test -n "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="$x_add_rpath:$LD_LIBRARY_PATH"
    else
        LD_LIBRARY_PATH="$x_add_rpath"
    fi
    export LD_LIBRARY_PATH

    case "`uname`" in
     Darwin*)
        DYLD_LIBRARY_PATH="$LD_LIBRARY_PATH"
        export DYLD_LIBRARY_PATH
        ;;
    esac    
}


COMMON_PrintDate()
{
    echo "[`date`]"
}


#
#  COMMON_AbsPath <var> <path>
#

COMMON_AbsPath()
{
    COMMON_AbsPath__dir=`dirname "$2"`
    COMMON_AbsPath__dir=`(cd "${COMMON_AbsPath__dir}" ; pwd)`
    eval $1="${COMMON_AbsPath__dir}"
}


COMMON_SetupScriptName()
{
    script_name=`basename $0`
    script_dir=`dirname $0`
    script_dir=`(cd "${script_dir}" ; pwd)`
}


COMMON_SetupRunDirCmd()
{
    run_dir=`pwd`
    run_cmd="$0 $*"
}


#
#  Post error message to STDERR and abort.
#  NOTE:  call "COMMON_SetupScriptName()" beforehand for nicer diagnostics.
#

COMMON_Error()
{
    {
        echo
        echo  "------------------------------------------------------"
        echo  "Current dir:  `pwd`"
        echo
        if [ "x$1" = "x-s" ]; then
            echo "[$script_name] FAILED (status $2):"
            shift
            shift
        else
            echo "[$script_name] FAILED:"
        fi
        err="   $1"
        shift
        for arg in "$@" ; do
            arg=`echo "$arg" | sed "s%'%\\\\\'%g"`
            err="$err '$arg'"
        done
        echo "$err"
    } 1>&2

    exit 1
}


#
#  COMMON_RollbackAndError(...)
#
#  Before aborting, executes the rollback procedure if $x_common_rb is
#  defined, otherwise, this function is equivalent to COMMON_Error.
#

COMMON_RollbackAndError()
{
    test -n "$x_common_rb" && $x_common_rb

    COMMON_Error "$@"
}


#
#  COMMON_CheckNArgsRB(fn, minn, maxn, arg, ...)
#
#  Verifies that the number of arguments (arg, ...) passed
#  to the "fn" function is >= "minn" and is <= "maxn". Either
#  of two boundaries can be omitted by specifying an empty string.
#

COMMON_CheckNArgsRB()
{
    fn="$1"; shift
    minn="$1"; shift
    maxn="$1"; shift

    test -z "$minn" || test "$#" -ge "$minn" || COMMON_RollbackAndError \
        "$fn($@): insufficient number of arguments ($# < $minn)"

    test -z "$maxn" || test "$#" -le "$maxn" || COMMON_RollbackAndError \
        "$fn($@): too many arguments ($# > $maxn)"
}


#
#  Execute a command;  on error, post error message to STDERR and abort.
#  NOTE:  call "COMMON_SetupScriptName()" beforehand for nicer diagnostics.
#

COMMON_Exec()
{
    "$@"

    x_status=$?
    if test $x_status -ne 0 ; then
        COMMON_Error -s $x_status "$@"
    fi
}


#  Variant of COMMON_Exec with RollBack functionality
#  In case of error checks x_common_rb variable and if it has been set
#  runs it as a rollback command, then posts error message to STDERR and abort.
#  NOTE:  call "COMMON_SetupScriptName()" beforehand for nicer diagnostics.
#

COMMON_ExecRB()
{
    "$@"

    x_status=$?
    if test $x_status -ne 0 ; then
        COMMON_RollbackAndError -s $x_status "$@"
    fi
}


#
#  COMMON_List_Unshift(listname, value)
#
#  Inserts a new element with the specified value at the beginning
#  of the "listname" list.
#

COMMON_List_Unshift()
{
    COMMON_CheckNArgsRB 'COMMON_List_Unshift' 2 2 "$@"

    listname="$1"
    value="$2"

    if eval "test -n \"\$$listname\""; then
        eval "$listname=\"$value
\$$listname\""
    else
        eval "$listname=\"$value\""
    fi
}


#
#  COMMON_List_Push(listname, value)
#
#  Appends a new element with the specified value to
#  the "listname" list.
#

COMMON_List_Push()
{
    COMMON_CheckNArgsRB 'COMMON_List_Push' 2 2 "$@"

    listname="$1"
    value="$2"

    if eval "test -n \"\$$listname\""; then
        eval "$listname=\"\$$listname
$value\""
    else
        eval "$listname=\"$value\""
    fi
}


#
#  COMMON_List_ForEach(listname, fn)
#
#  Iterates through the list specified by "listname" and executes
#  shell command "fn" with list element values as its last arguments.
#

COMMON_List_ForEach()
{
    COMMON_CheckNArgsRB 'COMMON_List_ForEach' 2 2 "$@"

    listname="$1"
    fn="$2"

    eval "echo \"\$$listname\"" | while read el; do
        $fn "$el"
    done
}


#
#  COMMON_RegisterInstalled(pathname)
#
#  Registers the file specified by the "pathname" argument as
#  freshly installed (that is, previously non-existent).
#

COMMON_RegisterInstalled()
{
    COMMON_CheckNArgsRB 'COMMON_RegisterInstalled' 1 1 "$@"

    COMMON_List_Unshift 'x_common_uninstall' "$1"
}


#
#  COMMON_RegisterInstalledDir(pathname)
#
#  Registers the directory specified by the "pathname" argument as
#  newly created (that is, previously non-existent).
#

COMMON_RegisterInstalledDir()
{
    COMMON_CheckNArgsRB 'COMMON_RegisterInstalledDir' 1 1 "$@"

    COMMON_List_Unshift 'x_common_uninstall_dirs' "$1"
}


#
#  COMMON_InstallRB(cmd, src, dest)
#
#  Installs the file specified by the "src" pathname to the location
#  specified by the "dest" file or directory name. The installation
#  is done by executing the "cmd" command, which can be a shell
#  command or an external script.
#

COMMON_InstallRB()
{
    COMMON_CheckNArgsRB 'COMMON_InstallRB' 3 3 "$@"

    cmd="$1"
    src="$2"
    dest="$3"

    test -r "$src" || COMMON_RollbackAndError "Cannot read '$src'"

    if test -d "$dest"; then
        destpathname="$dest/`basename "$src"`"
    else
        destpathname="$dest"
    fi

    if env test -e "$destpathname"; then
        COMMON_ExecRB mv -f "$destpathname" "$destpathname.save"
    fi

    COMMON_ExecRB $cmd "$src" "$dest"

    COMMON_RegisterInstalled "$destpathname"
}


#
#  COMMON_InstallDirRB(cmd, dir)
#
#  Creates the "dir" dirctory by means of executing the "cmd" command.
#

COMMON_InstallDirRB()
{
    COMMON_CheckNArgsRB 'COMMON_InstallDirRB' 2 2 "$@"

    cmd="$1"
    dir="$2"

    if test ! -d "$dir"; then
        tmpdirlist=''

        COMMON_List_Push 'tmpdirlist' "$dir"
        currentdir="$dir"
        parentdir="`dirname "$currentdir"`"

        while test "$currentdir" != "$parentdir" -a ! -d "$parentdir"; do
            COMMON_List_Push 'tmpdirlist' "$parentdir"
            currentdir="$parentdir"
            parentdir="`dirname "$currentdir"`"
        done

        COMMON_ExecRB $cmd "$dir"

        COMMON_RegisterInstalledDir "$tmpdirlist"
    fi
}


#
#  COMMON_CommitInstall()
#
#  Commits changes to the file system made by the COMMON_InstallRB and
#  COMMON_InstallDirRB functions by removing the .save files and internal
#  "rollback" information.
#

COMMON_CommitInstall()
{
    x_CommitFile()
    {
        rm -f "$1.save"
    }

    COMMON_List_ForEach 'x_common_uninstall' x_CommitFile 2> /dev/null

    unset x_common_uninstall x_common_uninstall_dirs
}


#
#  COMMON_RollbackInstall()
#
#  Uninstalls files and directories that have been installed so far by
#  COMMON_InstallRB and COMMON_InstallDirRB.
#

COMMON_RollbackInstall()
{
    x_RollbackFile()
    {
        env test -e "$1.save" && mv -f "$1.save" "$1" || rm -f "$1"
    }

    COMMON_List_ForEach 'x_common_uninstall' x_RollbackFile 2> /dev/null

    unset x_common_uninstall

    COMMON_List_ForEach 'x_common_uninstall_dirs' rmdir 2> /dev/null

    unset x_common_uninstall_dirs
}


#
#  Limit size of the text file to specified number of Kbytes.
#
#  $1 - source file;
#  $2 - destination file;
#  $3 - max.size of the destination file in Kbytes.
#
#  Return size of output file in Kbytes or zero on error.
#
# Some 'head' command implementations have '-c' parameter to limit
# size of output in bytes, but not all. 

COMMON_LimitTextFileSize()
{
    file_in="$1"
    file_out="$2"
    maxsize="${3:-0}"
    
    test $maxsize -lt 1  &&  return 0
    cp $file_in $file_out 1>&2
    test $? -ne 0  &&  return 0

    size=`wc -c < $file_out`
    size=`expr $size / 1024`
    test $size -lt $maxsize  &&  return $size
    
    lines=`wc -l < $file_out`
    while [ $size -ge $maxsize ]; do
        lines=`expr $lines / 2`
        head -n $lines $file_in > $file_out
        {
            echo
            echo "..."
            echo "[The size of output is limited to $maxsize Kb]"
        } >> $file_out
        size=`wc -c < $file_out`
        size=`expr $size / 1024`
        test $lines -eq 1  &&   break
    done

    return $size
}

#
#  Output a string indicating NCBI's short in-house name for the
#  current OS/CPU platform.
#
COMMON_DetectPlatform()
{
    raw_platform=`uname -sm`
    case "$raw_platform" in
	*CYGWIN_NT*86   ) echo Win32      ;;
	*CYGWIN_NT*64   ) echo Win64      ;; # unverified
	Darwin\ i386    ) echo IntelMAC   ;;
	Darwin\ x86_64  ) echo IntelMAC   ;; # split into IntelMac64?
	Darwin\ powerpc ) echo PowerMAC   ;;
	FreeBSD\ amd64  ) echo FreeBSD64  ;;
	FreeBSD\ i386   ) echo FreeBSD32  ;;
	IRIX64\ *       ) echo IRIX64     ;;
	Linux\ i?86     ) echo Linux32    ;;
	Linux\ x86_64   ) echo Linux64    ;;
	SunOS\ i*86*    ) echo SunOSx86   ;;
	SunOS\ sun4*    ) echo SunOSSparc ;;
	* )
	    echo "Platform not defined for $raw_platform -- please fix me" >&2
	    echo UNKNOWN
	    ;;
    esac
}
