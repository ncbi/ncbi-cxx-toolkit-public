# $Id$

APP = tds95_collations
SRC = collations common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

# Per upstream, really a utility, and as such not worth running as a test.
# CHECK_CMD  = test-tds95 tds95_collations

# CHECK_REQUIRES = in-house-resources

WATCHERS = ucko
