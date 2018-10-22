APP=deployable_cgi.cgi
SRC=sample process

### BEGIN COPIED SETTINGS
## Use these two lines for normal CGI.
LIB = xcgi xhtml xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
## Use these two lines for FastCGI.  (No other changes needed!)
# LIB = xfcgi xhtml xconnect xutil xncbi
# LIBS = $(FASTCGI_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
### END COPIED SETTINGS

WATCHERS=fukanchi
