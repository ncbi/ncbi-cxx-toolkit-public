#!/bin/sh

test -n "$NCBICXX_TESTING_REQS"  &&  exit 0

warn() {
    fmt >&2 <<EOF
Your build tree appears to be out of date relative to the C++
Toolkit's configuration script; to correct this, please run

    ./reconfigure.sh reconf

from your top build directory ($1) after ensuring that all relevant
environment variables (particularly PATH) are set correctly.
EOF
}

# XXX - implement ask/prompt?
case "$NCBICXX_RECONF_POLICY" in
    auto)
	cd $1  &&  exec sh reconfigure.sh reconf
	;;
    warn)
	warn $1
	echo
	echo "Proceeding anyway."
	exit 0
	;; 
    *) 
	warn $1
	exit 1
	;;
esac
