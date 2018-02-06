# $Id$
##################################
BINCOPY = @:
SCRIPT_NAME = get_species_taxids.sh

CHECK_REQUIRES = in-house-resources Linux MacOS
CHECK_COPY = ${SCRIPT_NAME}
CHECK_CMD = ${SCRIPT_NAME} -t 9606
CHECK_CMD = ${SCRIPT_NAME} -t mouse

WATCHERS = fongah2 camacho
