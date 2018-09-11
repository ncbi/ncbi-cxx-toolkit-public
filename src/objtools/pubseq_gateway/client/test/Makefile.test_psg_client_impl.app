# $Id$

APP = test_psg_client_impl
SRC = test_psg_client_impl
LIB = psg_client $(SEQ_LIBS) pub medline biblio general xser xconnserv xconnect test_boost xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(LIBUV_INCLUDE) $(NGHTTP2_INCLUDE) $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_psg_client_impl

WATCHERS = sadyrovr
