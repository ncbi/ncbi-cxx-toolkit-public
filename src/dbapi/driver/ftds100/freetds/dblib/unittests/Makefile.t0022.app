# $Id$

APP = db100_t0022
SRC = t0022 common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS100_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds100$(STATIC) tds_ftds100$(STATIC)
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Work around a known server bug observed when using TDS 7.x, under which
# the test either fails outright (with TDS 7.3) or self-disables (7.0-7.2).
CHECK_CMD  = test-db100 --ms-ver 4.2 --no-auto db100_t0022
CHECK_COPY = t0022.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
