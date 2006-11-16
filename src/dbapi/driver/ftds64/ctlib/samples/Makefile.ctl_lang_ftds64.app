# $Id$

APP = ctl_lang_ftds64
SRC = ctl_lang_ftds64

LIB  = ncbi_xdbapi_ftds64_ctlib$(STATIC) ct_$(ftds64)$(STATIC) tds_$(ftds64)$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

# CHECK_CMD = run_sybase_app.sh ctl_lang_ftds64
