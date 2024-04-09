# $Id$

APP = db14_batch_stmt_ins_upd
SRC = batch_stmt_ins_upd common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS14_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db14 db14_batch_stmt_ins_upd
CHECK_COPY = batch_stmt_ins_upd.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
