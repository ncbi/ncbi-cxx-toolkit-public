APP = test_pin
SRC = test_pin
LIB = seqdb xobjutil blastdb $(OBJMGR_LIBS)

CPPFLAGS      = $(ORIG_CPPFLAGS) -I$(NCBI)/include
CFLAGS        = $(FAST_CFLAGS)
CXXFLAGS      = $(FAST_CXXFLAGS)
LOCAL_LDFLAGS = $(FAST_LDFLAGS) -L$(NCBI)/ncbi/lib

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) -lncbitool -lncbiobj -lncbi

REQUIRES = objects

