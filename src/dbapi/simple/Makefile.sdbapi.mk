# $Id$

SDBAPI_LIB  = sdbapi ncbi_xdbapi_ftds $(FTDS_LIB) \
              ncbi_xdbapi_ftds14 $(FTDS14_LIB) \
              dbapi dbapi_util_blobstore dbapi_driver \
              $(XCONNEXT) $(COMPRESS_LIBS)
SDBAPI_LIBS = $(FTDS14_LIBS) $(CMPRS_LIBS)
SDBAPI_STATIC_LIBS = $(FTDS14_LIBS) $(CMPRS_STATIC_LIBS)
