# $Id$

APP = odbc_lang_ftds64
SRC = odbc_lang_ftds64

LIB  = ncbi_xdbapi_$(ftds64)_odbc$(STATIC) odbc_$(ftds64)$(STATIC) tds_$(ftds64)$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_COPY = run_sample_odbc.sh
CHECK_CMD = run_sample_odbc.sh odbc_lang_ftds64
