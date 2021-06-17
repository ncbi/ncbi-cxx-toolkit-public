#!/bin/sh

BIN_DIR=$1
SRC_DIR=$2
INSTALL_DIR=$3

CGI_NAME="test_stat_ext.cgi"
CGI_PATH="src/internal/cppcore/test_stat_ext/cgi"
CGI_XSL_DIR="xsl"
CGI_OVERLIB_DIR="overlib"
CGI_INIFILE_NAME="test_stat_ext.cgi.ini"
SVN_LOGS_NAME="svn_logs.cgi"


if [[ "$(uname -s)" == CYGWIN* ]]; then
    CGI_NAME="${CGI_NAME}.exe"
fi


if [ ! -d "$BIN_DIR" -o ! -d "$SRC_DIR" -o ! -d "$INSTALL_DIR" ]; then
    echo "Parameters given ('$BIN_DIR' and '$INSTALL_DIR') are not directories" >&2
    exit 1
fi

if [ -f "$BIN_DIR/$CGI_NAME" ]; then
    cp    "$BIN_DIR/$CGI_NAME" "$INSTALL_DIR/" || exit 2
    cp -R "$SRC_DIR/$CGI_PATH/$CGI_XSL_DIR" "$INSTALL_DIR/" || exit 3
    cp -R "$SRC_DIR/$CGI_PATH/$CGI_OVERLIB_DIR" "$INSTALL_DIR/" || exit 4
    cp    "$SRC_DIR/$CGI_PATH/$CGI_INIFILE_NAME" "$INSTALL_DIR/" || exit 5
    cp    "$SRC_DIR/$CGI_PATH/$SVN_LOGS_NAME" "$INSTALL_DIR/" || exit 6
fi

exit 0
