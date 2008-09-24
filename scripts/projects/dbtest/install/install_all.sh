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
                     "*Win64*"
                     "*Linux32*"
                     "*FreeBSD32*"
                     "*PowerMAC*"
                     "*SunOSx86*"
                     "*SunOSSparc*")
declare -a PLATF_DIR_NAMES
PLATF_DIR_NAMES=(    "Linux64"
                     "Win32"
                     "Linux32"
                     "FreeBSD32"
                     "PowerMAC"
                     "SunOSx86"
                     "SunOSSparc"
                     "Irix")
declare -a PLATF_SERVERS
PLATF_SERVERS=(      "coremake2"
                     "coremake2"
                     "coremake2"
                     "beastie"
                     "coremake2"
                     "coremake2"
                     "coremake2")
declare -a PLATF_NCBI_BIN_DIRS
PLATF_NCBI_BIN_DIRS=("/net/coleman/vol/export3/lnx64_netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/net/snowman/vol/export2/win-coremake/Builds/bin"
                     "/net/snowman/vol/export2/lnx_netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/netopt/ncbi_tools/bin/_production/CPPCORE"
                     "/net/coleman/vol/export3/darwin/ncbi_tools/bin/_production/CPPCORE"
                     "/net/coleman/vol/export3/ncbi_tools.solaris/i386-5.10/bin/_production/CPPCORE"
                     "/net/coleman/vol/export3/ncbi_tools.solaris/sparc-5.10/bin/_production/CPPCORE")

CGI_BIN_DIR="/net/snowman/vol/export2/iweb/ieb/ToolBox/STAT/test_stat"


VERSION=$1
shift
if [[ -z "$VERSION" ]]; then
    echo "Usage: install_all.sh <version>"
    exit 1
fi


PREPARE_DIR="$(find . -type d -name "prepare_release*build*")"
if [[ ! -d "$PREPARE_DIR" ]]; then
    echo "Cannot find directory made by prepare_release build!!!"
    exit 4
fi
cd "$PREPARE_DIR"


ATTIC_DIR="/net/attic1/vol/vol1/release-repository/dbtest/builds/${VERSION}"
ATTIC_SRC_DIR="/net/attic1/vol/vol1/release-repository/dbtest/src/${VERSION}"
mkdir -p "${ATTIC_DIR}"
mkdir -p "${ATTIC_SRC_DIR}"


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
    mkdir -p "${PLATF_DIR}"
    PLATF_ATTIC_DIR="${ATTIC_DIR}/${PLATF_DIR_NAMES[$i]}"
    mkdir -p "${PLATF_ATTIC_DIR}"
    tar -zxf "${PLATF_FILE}" -C "${PLATF_DIR}" || exit 6

    tar -zxf "${PLATF_SRC_FILE}" -C "${ATTIC_SRC_DIR}" || exit 16

    EXE=""
    if [[ "${PLATF_FILE_MASKS[$i]}" == *"Win64"* ]]; then
        EXE=".exe"
    fi

    cat "${PLATF_DIR}/bin/test_stat_load${EXE}" | ssh coremake@"${PLATF_SERVERS[$i]}" "cat >${PLATF_NCBI_BIN_DIRS[$i]}/test_stat_load${EXE}" || exit 7
    cp  "${PLATF_DIR}/bin/test_stat_load${EXE}" "${PLATF_ATTIC_DIR}/" || exit 8


    if [[ "${PLATF_FILE_MASKS[$i]}" == *"Linux64"* ]]; then
        echo "Deploying cgi interface"

        cp "${PLATF_DIR}/bin/test_stat_ext.cgi" "${CGI_BIN_DIR}/" || exit 9
        cp "${PLATF_DIR}/bin/test_stat_ext.cgi" "${PLATF_ATTIC_DIR}/" || exit 10
        cp -R "${PLATF_DIR}/bin/xsl/" "${CGI_BIN_DIR}/" || exit 11
        cp -R "${PLATF_DIR}/bin/xsl/" "${PLATF_ATTIC_DIR}/" || exit 12
        cp -R "${PLATF_DIR}/bin/overlib/" "${CGI_BIN_DIR}/" || exit 13
        cp -R "${PLATF_DIR}/bin/overlib/" "${PLATF_ATTIC_DIR}/" || exit 14

        touch "${CGI_BIN_DIR}/.sink_subtree"
    fi

    rm -rf "${PLATF_DIR}"
done


echo "Going to cruncher"

ssh coremake@cruncher "bash -s" <<EOF || exit 15

mkdir -p "$TMP_DIR"
cd "$TMP_DIR" || exit 1

svn co "https://svn.ncbi.nlm.nih.gov/repos/toolkit/release/dbtest/${VERSION}" ./ || exit 2
CC=cc CXX=CC ./configure --with-dll --without-debug --with-64 --with-flat-makefile || exit 3

cp -R "./c++/MIPSpro73-ReleaseDLL64" "${ATTIC_SRC_DIR}/c++/" || exit 9

gmake -j 5 || exit 4

for i in \`find ./c++/MIPSpro73-ReleaseDLL64/lib/ -name "*.so" | egrep -v "odbc_ftds64|sybdb_ftds64|test_boost|test_mt|xcgi|xfcgi|xthrserv"\`; do
    cp "\$i" "\$NCBI/bin/_production/CPPCORE/" || exit 4
done

mkdir -p "${ATTIC_DIR}/${PLATF_DIR_NAMES[7]}/"

cp "./c++/MIPSpro73-ReleaseDLL64/bin/test_stat_load" "\$NCBI/bin/_production/CPPCORE/" || exit 5
cp "./c++/MIPSpro73-ReleaseDLL64/bin/test_stat_load" "${ATTIC_DIR}/${PLATF_DIR_NAMES[7]}/" || exit 6
cp "./c++/src/internal/cppcore/test_stat_ext/loader/test_stat_load.sh" "\$NCBI/bin/_production/CPPCORE/" || exit 7
cp "./c++/src/internal/cppcore/test_stat_ext/loader/test_stat_load.sh" "${ATTIC_DIR}/${PLATF_DIR_NAMES[7]}/" || exit 8

mkdir -p "${ATTIC_SRC_DIR}/${PLATF_DIR_NAMES[7]}/"
ln -s "../c++" "${ATTIC_SRC_DIR}/${PLATF_DIR_NAMES[7]}/c++"

cd
rm -rf "$TMP_DIR"

EOF

echo "Everything installed successfully"
