#!/bin/sh
awk 'BEGIN { keep=1 }
/^[^ ]/ { keep=1 }
/^cc-[0-9]+ CC: (REMARK|WARNING) File = \/usr\/include/ { keep=0 }
keep

BEGIN { status=0 }
/^cc-[0-9]+ CC: ERROR/ { status=1 }
/^CC ERROR:/ { status=1 }
END { exit status }'
