# $Id$
##################################

ifeq (0,1)
	APP = update_blastdb.pl
endif

WATCHERS = madden camacho
CHECK_COPY = update_blastdb.pl
CHECK_CMD = perl -c update_blastdb.pl
