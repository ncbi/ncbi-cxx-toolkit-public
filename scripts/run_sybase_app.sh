#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
# 
#  Run SYBASE application under MS Windows.
#  To run it under UNIX use configurable script "run_sybase_app.sh"
#  in build dir.
#
###########################################################################


SYBASE="C:\\Sybase"
export SYBASE
exec "$@"
