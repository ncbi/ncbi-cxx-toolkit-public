#! /bin/sh

# $Id$
# Authors:  Denis Vakatov, NCBI; Aaron Ucko, NCBI
#
# Temporary wrapper for the new central WorkShop redundant warning filter.

exec `dirname $0`/cxx_filter.WorkShop.sh
