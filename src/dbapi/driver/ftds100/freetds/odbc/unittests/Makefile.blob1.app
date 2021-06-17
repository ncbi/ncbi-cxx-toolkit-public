# $Id$

APP = odbc100_blob1
SRC = blob1 common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS100_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = odbc_ftds100$(STATIC) tds_ftds100$(STATIC) odbc_ftds100$(STATIC)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Some systems would normally send data too fast(!) to MSSQL servers,
# and proceed to get throttled altogether; the dumping code adds
# enough of a slowdown to keep them out of trouble.
CHECK_CMD  = test-odbc100 --set-env TDSDUMP=/dev/null odbc100_blob1
CHECK_COPY = odbc.ini

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
