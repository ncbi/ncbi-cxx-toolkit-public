# $Id$

REQUIRES = objects

APP = test_asn_converter
SRC = test_asn_converter

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

LIB  = seqset $(SEQ_LIBS) pub medline biblio general xser xutil xconnect xncbi
LIBS = $(NCBI_C_LIBPATH) -lncbiobj $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
