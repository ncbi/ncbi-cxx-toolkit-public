# $Id$
##################################

ifeq (0,1)
	APP = legacy_blast.pl
endif

WATCHERS = madden camacho
CHECK_COPY = legacy_blast.pl
CHECK_CMD = perl -c legacy_blast.pl
