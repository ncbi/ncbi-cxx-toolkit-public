# $Id$

APP = ftds_sp_who
SRC = ftds_sp_who

LIB  = ncbi_xdbapi_$(ftds8)$(STATIC) $(FTDS8_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS8_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_COPY = ftds_sp_who.ini
CHECK_CMD =
