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
        if [ ! -z "$x_common_rb" ]; then
            $x_common_rb
        fi
        COMMON_Error -s $x_status "$@"
    fi
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
