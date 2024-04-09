# $Id$

APP = odbc100_describecol
SRC = describecol common parser

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS100_INCLUDE) \
           $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = odbc_ftds100 tds_ftds100 odbc_ftds100
LIBS     = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-odbc100 odbc100_describecol
CHECK_COPY = odbc.ini describecol.in

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
