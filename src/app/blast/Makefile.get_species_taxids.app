# $Id$
##################################

ifeq (0,1)
	APP = get_species_taxids.sh
endif

CHECK_REQUIRES = in-house-resources -MSWin -BSD
CHECK_COPY = get_species_taxids.sh
CHECK_CMD = get_species_taxids.sh -t 9606
CHECK_CMD = get_species_taxids.sh -t mouse

WATCHERS = fongah2 camacho
