# $Id$


APP = alnmrg
SRC = alnmrg

LIB = xalnmgr $(OBJREAD_LIBS) ncbi_xloader_blastdb seqdb xobjutil blastdb \
      tables $(OBJMGR_LIBS) $(LMDB_LIB)

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) \
       $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = run_sybase_app.sh -run-script alnmrg.sh /CHECK_NAME=alnmrg.sh
CHECK_COPY = alnmrg.sh data

WATCHERS = grichenk
