# $Id$

REQUIRES = dbapi

APP = alnmrg
SRC = alnmrg
LIB = xalnmgr submit tables $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

# CHECK_CMD = alnmrg.sh
# CHECK_COPY = alnmrg.sh data
