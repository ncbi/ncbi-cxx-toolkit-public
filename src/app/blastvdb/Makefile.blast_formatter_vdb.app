WATCHERS = camacho madden fongah2

APP = blast_formatter_vdb
SRC = blast_vdb_app_util blast_formatter_vdb

CPPFLAGS = -I$(LOCAL_TOOLKIT_PATH)/include \
            $(ORIG_CPPFLAGS) \
           -I$(srcdir) \
           -I$(includedir)/internal/blast/vdb \
           $(SRA_INCLUDE)
LDFLAGS += -L$(import_root)/../../../c++/lib $(OPENMP_FLAGS)

LIB_ = vdb2blast $(SRAREAD_LDDEP) $(SRAREAD_LIBS) $(BLAST_INPUT_LIBS) \
$(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = blast_app_util ncbi_xloader_wgs ncbi_xloader_csra $(LIB_:%=%$(STATIC))

# More standard libs (do we really need all these?)
LIBS += $(GENBANK_THIRD_PARTY_LIBS) $(VDB_STATIC_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)
