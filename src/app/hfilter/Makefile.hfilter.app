# $Id$

WATCHERS = badrazat

APP = hfilter
SRC = hitfilter_app

LIB = xalgoalignutil xqueryparse xalnmgr xobjutil tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
