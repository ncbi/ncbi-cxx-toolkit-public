#! /bin/sh

# $Id$

# Transitional wrapper script to avoid a chicken-and-egg situation.
exec "`dirname $0`/common/impl/reconfigure.sh" "$@"
