# $Id$
##################################
BINCOPY = @:
SCRIPT_NAME = update_blastdb.pl

WATCHERS = madden camacho
CHECK_COPY = ${SCRIPT_NAME}
CHECK_CMD = perl -c ${SCRIPT_NAME}
