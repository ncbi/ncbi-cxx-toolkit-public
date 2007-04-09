# $Id$

APP = dbl_sp_who_ftds64
SRC = dbl_sp_who_ftds64

LIB  = ncbi_xdbapi_$(ftds64)_dblib$(STATIC) sybdb_$(ftds64)$(STATIC) tds_$(ftds64)$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

# CHECK_CMD = run_sybase_app.sh dbl_sp_who_ftds64
