APP = test_pin
SRC = test_pin
LIB = seqdb xobjutil blastdb $(OBJMGR_LIBS)

CPPFLAGS      = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
CFLAGS        = $(FAST_CFLAGS)
CXXFLAGS      = $(FAST_CXXFLAGS)

LIBS = $(NCBI_C_LIBPATH) -lncbitool -lncbiobj $(NCBI_C_ncbi) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects C-Toolkit
