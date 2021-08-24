# $Id$

# Build test application "test_jaeger"
#################################

APP = test_jaeger
SRC = test_jaeger

CPPFLAGS = $(ORIG_CPPFLAGS) $(JAEGER_INCLUDE) $(BOOST_INCLUDE)
LIB = xncbi jaeger_tracer
LIBS = $(JAEGER_LIBS) $(ORIG_LIBS)

REQUIRES = JAEGER

CHECK_CMD  =

WATCHERS = grichenk
