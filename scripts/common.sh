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
     IRIX*)
        _RLD_ROOT="/usr"
        export _RLD_ROOT
        ;;
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
        echo "[$script_name] FAILED:"
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

    if test $? -ne 0 ; then
        COMMON_Error "$@"
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

    if test $? -ne 0 ; then
        if [ ! -z "$x_common_rb" ]; then
            $x_common_rb
        fi
        COMMON_Error "$@"
    fi
}
