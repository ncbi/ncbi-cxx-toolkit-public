# $Id$

APP = blast_vdb_cmd
SRC = blast_vdb_cmd

CPPFLAGS = -I$(LOCAL_TOOLKIT_PATH)/include \
            $(ORIG_CPPFLAGS) \
           -I$(srcdir) \
           -I$(includedir)/internal/blast/vdb \
           $(SRA_INCLUDE)
CXXFLAGS += $(OPENMP_FLAGS)

# Standard libs for Blast, Sequences, etc
LIB_ = vdb2blast ncbi_xloader_csra $(SRAREAD_LDDEP) $(SRAREAD_LIBS) $(BLAST_INPUT_LIBS) \
$(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LDFLAGS += $(OPENMP_FLAGS)


# More standard libs (do we really need all these?)
LIBS += $(VDB_STATIC_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

WATCHERS = fongah2 camacho madden
