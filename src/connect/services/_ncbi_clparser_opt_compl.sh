# $Id$
#
# Bash completion helper for _ncbi_clparser_completion.
#
# Author: Dmitry Kazimirov <kazimird@ncbi.nlm.nih.gov>

_invoke_lbsmc()
{
    "$2" -s "$1*" -h none -x 0
}

_invoke_lbsmc_cgi()
{
    local prefix="$1"
    shift
    local host='intranet.ncbi.nlm.nih.gov'
    "$@" "http://$host/ieb/ToolBox/NETWORK/lbsmc.cgi?-s+$prefix*+-x+-h+none"
}

_lookup_lbsm_service()
{
    if which wget; then
        _invoke_lbsmc_cgi "$1" wget -q -O -
    elif which curl; then
        _invoke_lbsmc_cgi "$1" curl -f -o -
    elif which lbsmc 2> /dev/null >&2; then
        _invoke_lbsmc "$1" lbsmc
    elif which /opt/machine/lbsm/bin/lbsmc 2> /dev/null >&2; then
        _invoke_lbsmc "$1" /opt/machine/lbsm/bin/lbsmc
    fi | perl -wne 'm,Svcname>(.+)</Svcname, && print "$1\n"'
}

_ncbi_clparser_opt_compl()
{
    local program="$1"
    local opt="$2"
    local prefix="$3"

    if [[ $program = 'grid_cli' ]]; then
        case "$opt" in
        --netschedule | --ns)
            if [[ $prefix ]]; then
                case "$prefix" in
                n | N | ns | Ns | nS | NS)
                    prefix='NS_'
                    ;;
                esac
            else
                prefix='NS_'
            fi
            #_lookup_lbsm_service "$prefix"
            ;;
        --netcache | --nc)
            if [[ $prefix ]]; then
                case "$prefix" in
                i | I | ic | Ic | iC | IC)
                    prefix='IC_'
                    ;;
                n | N | nc | Nc | nC | NC)
                    prefix='NC_'
                    ;;
                esac
            else
                prefix='?C_'
            fi
            #_lookup_lbsm_service "$prefix"
            ;;
        --input-file | --output-file)
            ;;
        *)
            [[ ! $prefix ]] && echo ARG
        esac
    fi
}
