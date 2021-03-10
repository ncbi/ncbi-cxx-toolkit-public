# $Id$

SDBAPI_LIB  = sdbapi ncbi_xdbapi_ftds $(FTDS_LIB) \
              ncbi_xdbapi_ftds100 $(FTDS100_LIB) \
              dbapi dbapi_util_blobstore dbapi_driver \
              $(XCONNEXT) $(COMPRESS_LIBS)
SDBAPI_LIBS = $(FTDS95_LIBS) $(FTDS100_LIBS) $(CMPRS_LIBS)
