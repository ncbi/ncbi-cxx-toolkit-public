# $Id$

APP = ftds_sp_who63
SRC = ftds_sp_who

LIB  = ncbi_xdbapi_$(ftds63)$(STATIC) $(FTDS63_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS63_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS63_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_COPY = ftds_sp_who63.ini
CHECK_CMD =
