# $Id$
##################################

ifeq (0,1)
	APP = cleanup-blastdb-volumes.py
endif

WATCHERS = madden camacho
CHECK_COPY = cleanup-blastdb-volumes.py
CHECK_CMD = python3 -m unittest cleanup-blastdb-volumes
