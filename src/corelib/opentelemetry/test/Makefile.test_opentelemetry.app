# $Id$

# Build test application "test_opentelemetry"
#############################################

APP = test_opentelemetry
SRC = test_opentelemetry

CPPFLAGS = $(ORIG_CPPFLAGS) $(OPENTELEMETRY_INCLUDE) $(BOOST_INCLUDE)
LIB = xncbi opentelemetry_tracer
LIBS = $(OPENTELEMETRY_STATIC_LIBS) $(GRPC_LIBS) $(CURL_LIBS) $(Z_LIBS) \
       $(ORIG_LIBS)

REQUIRES = OPENTELEMETRY

CHECK_CMD = test_opentelemetry -file -

WATCHERS = grichenk
