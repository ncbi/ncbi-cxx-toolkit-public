APP=sample_cgi_test
SRC=sample_cgi_test

### BEGIN COPIED SETTINGS
CPPFLAGS=$(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

## Use these two lines for normal CGI.
LIB = xcgi xhtml xconnect xutil test_boost xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES=Boost.Test.Included

CHECK_CMD=sample_cgi_test

## Use these two lines for FastCGI.  (No other changes needed!)
# LIB = xfcgi xhtml xconnect xutil xncbi
# LIBS = $(FASTCGI_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
### END COPIED SETTINGS

WATCHERS=fukanchi

