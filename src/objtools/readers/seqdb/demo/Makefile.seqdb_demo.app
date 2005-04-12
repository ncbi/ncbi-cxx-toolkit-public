APP = seqdb_demo
SRC = seqdb_demo
LIB = seqdb xobjutil blastdb $(OBJMGR_LIBS)

CPPFLAGS      = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
CFLAGS        = $(FAST_CFLAGS)
CXXFLAGS      = $(FAST_CXXFLAGS)
LDFLAGS       = $(FAST_LDFLAGS)

NCBI_C_LIBS = -lncbimmdb -lncbitool -lncbiobj -lncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(ORIG_LIBS)

REQUIRES = objects

