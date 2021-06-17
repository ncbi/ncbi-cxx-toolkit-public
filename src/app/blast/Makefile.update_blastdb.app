# $Id$
##################################

ifeq (0,1)
	APP = update_blastdb.pl
endif

WATCHERS = madden camacho
CHECK_COPY = update_blastdb.pl
CHECK_CMD = perl -c update_blastdb.pl
CHECK_CMD = perl update_blastdb.pl --showall
CHECK_CMD = perl update_blastdb.pl --showall --source gcp
CHECK_CMD = perl update_blastdb.pl --showall --source ncbi
CHECK_CMD = perl update_blastdb.pl --showall --source aws

# Additional test cases: https://teamcity.ncbi.nlm.nih.gov/project/SB_UpdateBlastdbPl?mode=builds
