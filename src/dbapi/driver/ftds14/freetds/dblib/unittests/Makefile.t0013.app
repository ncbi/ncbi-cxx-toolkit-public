# $Id$

APP = db14_t0013
SRC = t0013 common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS14_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Needs automatic textptr metadata, gone as of TDS 7.2.
CHECK_CMD  = test-db14 --ms-ver 7.1 --no-auto db14_t0013
CHECK_COPY = t0013.sql data.bin

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse