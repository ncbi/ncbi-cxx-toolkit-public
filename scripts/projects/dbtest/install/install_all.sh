#!/usr/bin/bash
#
# Author: Pavel Ivanov
#
#
# This script will deploy test_stat_load and test_stat_ext.cgi into standard locations
# of these programs. Script should be launched under coremake account on coremake2 or any other
# machine that can freely (without password) log into coremake2 and beastie computers. Script
# should be launched in the following way:
#
# install_all.sh 0.0.0
#
# Where 0.0.0 is version of the release that should be deployed. Script should be launched from
# the directory where dbtest was built on all platforms, i.e. current directory should contain
# prepare_release* subdirectory left by prepare_release build script and it should be writable
# by coremake user.
#


TMP_DIR="/tmp/$$"


declare -a PLATF_FILE_MASKS
PLATF_FILE_MASKS=(   "*Linux64*"
                     "*Win32*"
                     "*Linux32*"
                     "*FreeBSD32*"
                     "*IntelMAC*"
                     "*SunOSx86*"
                     "*SunOSSparc*")
declare -a PLATF_DIR_NAMES
PLATF_DIR_NAMES=(    "Linux64"
                     "Win32"
                     "Linux32"
                     "FreeBSD32"
                     "IntelMAC"
                     "SunOSx86"
                     "SunOSSparc")
declare -a PLATF_SERVERS
PLATF_SERVERS=(      "coremake2"
                     "coremake2"
                     "coremake2"
                     "beastie3"
                     "coremake2"
                     "coremake2"
                     "coremake2")
declare -a PLATF_NCBI_BIN_DIRS
PLATF_NCBI_BIN_DIRS=("/net/napme02/vol/ncbi_tools/lnx64_netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/net/snowman/vol/export2/win-coremake/Builds/bin"
                     "/net/snowman/vol/export2/lnx_netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/net/snowman/vol/osx/ncbi_tools/bin/_production/CPPCORE"
                     "/net/snowman/vol/projects/ncbi_tools.solaris/i386-5.10/bin/_production/CPPCORE"
                     "/net/snowman/vol/projects/ncbi_tools.solaris/sparc-5.10/bin/_production/CPPCORE")

CGI_BIN_DIR="/net/iwebdev2/export/home/web/public/htdocs/ieb/ToolBox/STAT/test_stat"



PREPARE_DIR="$(find . -type d -name "prepare_release*build.dbtest*")"
if [[ ! -d "$PREPARE_DIR" ]]; then
    echo "Cannot find directory made by prepare_release build!!!"
    exit 4
fi
cd "$PREPARE_DIR"


for ((i = 0; i < 7; ++i)); do
    echo "Deploying ${PLATF_FILE_MASKS[$i]}"

    PLATF_FILE="$(find . -type f -name "${PLATF_FILE_MASKS[$i]}.tar.gz" -a ! -name "*-src*")"
    if [[ -z "$PLATF_FILE" ]]; then
        echo "Cannot find file for mask '${PLATF_FILE_MASKS[$i]}.tar.gz'"
        exit 5
    fi

    PLATF_SRC_FILE="$(find . -type f -name "${PLATF_FILE_MASKS[$i]}-src.tar.gz")"
    if [[ -z "$PLATF_SRC_FILE" ]]; then
        echo "Cannot find sources file for mask '${PLATF_FILE_MASKS[$i]}-src.tar.gz'"
        exit 5
    fi

    PLATF_DIR="${PLATF_FILE%%.tar.gz}"
    tar -zxf "${PLATF_FILE}" || exit 6

    EXE=""
    if [[ "${PLATF_FILE_MASKS[$i]}" == *"Win32"* ]]; then
        EXE=".exe"
    fi

    cat "${PLATF_DIR}/bin/test_stat_load${EXE}" | ssh coremake@"${PLATF_SERVERS[$i]}" "cat >${PLATF_NCBI_BIN_DIRS[$i]}/test_stat_load${EXE}" || exit 7
    if [[ -f "${PLATF_DIR}/bin/test_stat_load${EXE}.manifest" ]]; then
        cat "${PLATF_DIR}/bin/test_stat_load${EXE}.manifest" | ssh coremake@"${PLATF_SERVERS[$i]}" "cat >${PLATF_NCBI_BIN_DIRS[$i]}/test_stat_load${EXE}.manifest" || exit 8
    fi


    if [[ "${PLATF_FILE_MASKS[$i]}" == *"Linux64"* ]]; then
        echo "Deploying cgi interface"

        cp -p "${PLATF_DIR}/bin/test_stat_ext.cgi" "${CGI_BIN_DIR}/" || exit 9
        cp -p "${PLATF_DIR}/bin/svn_logs.cgi" "${CGI_BIN_DIR}/" || exit 10
        cp -Rp "${PLATF_DIR}/bin/xsl/" "${CGI_BIN_DIR}/" || exit 11
        cp -Rp "${PLATF_DIR}/bin/overlib/" "${CGI_BIN_DIR}/" || exit 13

        touch "${CGI_BIN_DIR}/.sink_subtree"
    fi

    rm -rf "${PLATF_DIR}"
done


echo "Everything installed successfully"
