#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = reader_pubseq_test
SRC = reader_pubseq_test

CPPFLAGS = \
  $(ORIG_CPPFLAGS) \
  -I$(srcdir) \
  -I$(srcdir)/../u2 \
  $(NCBI_C_INCLUDE) \
  $(NCBI_SSSDB_INCLUDE) \
  $(SYBASE_INCLUDE)

LIB = pubseqld id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver_samples dbapi_driver_ctlib dbapi_driver \
      xser xconnect xutil xncbi 

PRE_LIBS =  -L.. -lxobjmgr1

LIBS = $(SYBASE_DLLS) $(SYBASE_LIBS) $(SYBASE_DBLIBS) \
       $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

