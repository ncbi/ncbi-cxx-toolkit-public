#################################
# $Id$
#################################

APP = prosplign_demo
SRC = prosplign_demo

LIB = prosplign xobjutil \
      ncbi_xloader_lds lds bdb \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)


REQUIRES = objects bdb
