#
#  $Id$
#  ATTENTION!  This is to be 'included' (using ". common.sh'), not executed!
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
      IRIX*) _RLD_ROOT="/usr"
      export _RLD_ROOT
      ;;
    esac    
}
