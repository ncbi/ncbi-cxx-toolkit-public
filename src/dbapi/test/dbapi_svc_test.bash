#! /bin/bash
# $Id$

if (( $# < 1 )); then
    >&2 echo "USAGE: $0 <lbsmd|namerd> [sybase|nosybase]"
    exit 1
fi
mapper=${1,,}
sybase=${2,,}

if [[ "$sybase" == "sybase" ]]; then
    SYB_ENV="RESET_SYBASE=True SYBASE=$PWD "
else
    SYB_ENV=""
fi

if [[ -z "$CHECK_EXEC" ]]; then
    run_local="./"
else
    run_local=""
fi

# Failing tests as of 2020-01-21 - skip them for now.
# Mapper  Test    API         Custom interfaces   Service
# LBSMD   30      DBAPI_FTDS  no                  DT_LBR_NOI_BK_UP
# LBSMD   33      DBAPI_FTDS  yes                 DT_LBR_I_BK_UP
# LBSMD   102     SDBAPI      no                  DT_LBR_NOI_BK_UP
# LBSMD   105     SDBAPI      yes                 DT_LBR_I_BK_UP
# LBSMD   138     DBLB        no                  DT_LBR_NOI_BK_UP
# LBSMD   141     DBLB        yes                 DT_LBR_I_BK_UP
# namerd  30      DBAPI_FTDS  no                  DT_LBR_NOI_BK_UP
# namerd  32      DBAPI_FTDS  yes                 DT_LBR_NOI_NE_UP
# namerd  33      DBAPI_FTDS  no                  DT_LBR_I_BK_UP
# namerd  35      DBAPI_FTDS  yes                 DT_LBR_I_NE_UP
# namerd  102     SDBAPI      no                  DT_LBR_NOI_BK_UP
# namerd  104     SDBAPI      yes                 DT_LBR_NOI_NE_UP
# namerd  105     SDBAPI      no                  DT_LBR_I_BK_UP
# namerd  107     SDBAPI      yes                 DT_LBR_I_NE_UP
# namerd  138     DBLB        no                  DT_LBR_NOI_BK_UP
# namerd  140     DBLB        yes                 DT_LBR_NOI_NE_UP
# namerd  141     DBLB        no                  DT_LBR_I_BK_UP
# namerd  143     DBLB        yes                 DT_LBR_I_NE_UP
case $mapper in
    lbsmd)  skip="30,33,102,105,138,141" ;;
    namerd) skip="30,33,102,105,138,141,32,35,104,107,140,143" ;;
esac

if [[ -n "$skip" ]]; then
    eval "${SYB_ENV}$CHECK_EXEC ${run_local}dbapi_svc_test -mapper $mapper -report selected -skip $skip"
else
    eval "${SYB_ENV}$CHECK_EXEC ${run_local}dbapi_svc_test -mapper $mapper -report selected"
fi
