# $Id$

SDBAPI_LIB  = sdbapi ncbi_xdbapi_ftds $(FTDS_LIB) \
              ncbi_xdbapi_ftds64 $(FTDS64_LIB) \
              ncbi_xdbapi_ftds95 $(FTDS95_LIB) \
              dbapi dbapi_driver $(XCONNEXT)
SDBAPI_LIBS = $(FTDS64_LIBS) $(FTDS95_LIBS)
