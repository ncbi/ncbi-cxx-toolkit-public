# $Id$

APP = db95_t0017
SRC = t0017 common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS95_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds95$(STATIC) tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db95.sh db95_t0017
CHECK_COPY = t0017.sql t0017.in t0017.in.be

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko
