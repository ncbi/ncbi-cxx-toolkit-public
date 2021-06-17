# $Id$
# Author:  Sergey Satskiy (satskyse@ncbi.nlm.nih.gov)

# Build buffer writer test application "test_buffer_writer"
#################################

APP = test_buffer_writer
SRC = test_buffer_writer
LIB = xutil test_boost xncbi
REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

CHECK_CMD =

WATCHERS = satskyse
