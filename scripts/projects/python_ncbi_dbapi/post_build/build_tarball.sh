#!/bin/bash


INSTALL_DIR=$1
VERSION="${NCBI_PACKAGE_VERSION}"

if [[ -z "$INSTALL_DIR" || -z "$VERSION" ]]; then
    echo "Installation directory or version are empty"
    exit 1
fi

#PYTHON_DIR="$(which python)"
#PYTHON_DIR="${PYTHON_DIR%/*}"
#PYTHON_DIR="${PYTHON_DIR%/*}"
PYTHON_DIR=/opt/python-2.5.1

if [ -z "$PYTHON_DIR" ]; then
    echo "Cannot find python directory"
    exit 2
fi


PYTHON_SUBDIR="python2.5/site-packages/python_ncbi_dbapi"
TARBALL_DIR="$INSTALL_DIR/build_tarball"

mkdir -p "$TARBALL_DIR/$PYTHON_SUBDIR/$VERSION" || exit 3
cp -R "$PYTHON_DIR/lib/$PYTHON_SUBDIR"/* "$TARBALL_DIR/$PYTHON_SUBDIR/" || exit 4

cp "$INSTALL_DIR/lib"/* "$TARBALL_DIR/$PYTHON_SUBDIR/$VERSION" || exit 5
ln -s "python_ncbi_dbapi/$VERSION/libpython_ncbi_dbapi.so" "$TARBALL_DIR/$PYTHON_SUBDIR/../python_ncbi_dbapi.so" || exit 6

tar -zcf "$INSTALL_DIR/installation.tar.gz" -C "$TARBALL_DIR" python2.5 || exit 7

rm -rf "$TARBALL_DIR"
