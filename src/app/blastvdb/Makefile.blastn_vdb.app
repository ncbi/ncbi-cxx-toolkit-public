# $Id$

WATCHERS = camacho fongah2

APP = blastn_vdb
SRC = blast_vdb_app_util blastn_vdb

CPPFLAGS = -DNCBI_MODULE=BLASTVDB $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CXXFLAGS += $(OPENMP_FLAGS)
LDFLAGS += $(OPENMP_FLAGS)

LIB_ = vdb2blast $(SRAREAD_LDDEP) $(SRAREAD_LIBS) $(BLAST_INPUT_LIBS) \
$(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = blast_app_util ncbi_xloader_wgs ncbi_xloader_csra $(LIB_:%=%$(STATIC))

# More standard libs (do we really need all these?)
LIBS += $(BLAST_THIRD_PARTY_LIBS) $(GENBANK_THIRD_PARTY_LIBS) \
        $(VDB_STATIC_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)


