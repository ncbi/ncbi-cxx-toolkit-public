# $Id$


APP = alnmrg
SRC = alnmrg
LIB = xalnmgr xobjread ncbi_xloader_blastdb seqdb xobjutil submit blastdb \
      tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = run_sybase_app.sh alnmrg.sh /CHECK_NAME=alnmrg.sh
CHECK_COPY = alnmrg.sh data

WATCHERS = todorov
