# $Id$

APP = test_ncbi_sftp
SRC = test_ncbi_sftp
LIB = xconnsftp xconnect xutil test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(XCONNSFTP_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included LIBSSH Cxx2020_format_api

WATCHERS = sadyrovr
