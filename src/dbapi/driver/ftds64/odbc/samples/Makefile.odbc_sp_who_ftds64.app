# $Id$

APP = odbc_sp_who_ftds64
SRC = odbc_sp_who_ftds64

LIB  = ncbi_xdbapi_ftds64_odbc$(STATIC) odbc_$(ftds64)$(STATIC) tds_$(ftds64)$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_CMD = run_sybase_app.sh odbc_sp_who_ftds64
