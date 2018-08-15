# $Id$

APP = test_psg
SRC = test_psg
LIB = psg_client psg_rpc psg_diag $(SEQ_LIBS) pub medline biblio general xser xconnserv xconnect test_boost xutil xncbi

LIBS = $(PSG_RPC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(PSG_RPC_INCLUDE) $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_psg

WATCHERS = sadyrovr
