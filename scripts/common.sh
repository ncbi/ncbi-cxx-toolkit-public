#
#  $Id$
#  ATTENTION!  This is to be 'included' (using ". common.sh'), not executed!
#

COMMON_AddRunpath()
{
    add_rpath="$1"

    test -z "$add_rpath"  &&  return 0

    if test -n "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="$libdir:$LD_LIBRARY_PATH"
    else
        LD_LIBRARY_PATH="$libdir"
    fi
    export LD_LIBRARY_PATH

    case "`uname`" in
      IRIX*) _RLD_ROOT="/usr"
      export _RLD_ROOT
      ;;
    esac    
}
