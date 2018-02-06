# $Id$
##################################
BINCOPY = @:
SCRIPT_NAME = legacy_blast.pl

WATCHERS = madden camacho
CHECK_COPY = ${SCRIPT_NAME}
CHECK_CMD = perl -c ${SCRIPT_NAME}
