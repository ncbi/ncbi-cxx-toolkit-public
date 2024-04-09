# $Id$

APP = db14_spid
SRC = spid common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS14_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db14 db14_spid
CHECK_COPY = spid.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
