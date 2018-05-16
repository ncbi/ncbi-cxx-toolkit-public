APP=sample_cgi_test
SRC=sample_cgi_test

CPPFLAGS=$(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB=xcgi xhtml xconnect xutil test_boost xncbi
LIBS=$(CGI_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES=Boost.Test.Included

CHECK_CMD=sample_cgi_test

WATCHERS=fukanchi

