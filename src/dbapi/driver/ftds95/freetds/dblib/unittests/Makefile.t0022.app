# $Id$

APP = db95_t0022
SRC = t0022 common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS95_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds95$(STATIC) tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Work around a known server bug observed when using TDS 7.x, under which
# the test either fails outright (with TDS 7.3) or self-disables (7.0-7.2).
CHECK_CMD  = test-db95.sh --ms-ver 4.2 --no-auto db95_t0022
CHECK_COPY = test-db95.sh t0022.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko
