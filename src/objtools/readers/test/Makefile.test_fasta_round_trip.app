#################################
# $Id$
#################################

APP = test_fasta_round_trip
SRC = test_fasta_round_trip

LIB = xobjread xobjutil creaders $(SOBJMGR_LIBS)
LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = ucko

CHECK_CMD = test_fasta_round_trip.sh
CHECK_COPY = test_fasta_round_trip.sh test_fasta_round_trip_data
