#!/bin/sh
# Store the ultimate output in a temporary file to reduce the
# likelihood that it will end up interleaved with other files' output.
tmpout=${TMP:-/tmp}/ncbicxx.$$
trap "rm $tmpout" 0 1 2 15

awk 'BEGIN { keep=1 }
/^[^ ]/ { keep=1 }
/^cc-[0-9]+ CC: (REMARK|WARNING) File = \/usr\/include/ { keep=0 }
keep

BEGIN { status=0 }
/^cc-[0-9]+ CC: ERROR/ { status=1 }
/^CC ERROR:/ { status=1 }
END { exit status }' > $tmpout
status=$?

cat $tmpout
exit $status
