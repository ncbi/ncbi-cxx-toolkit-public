# $Id$
# Authors:  Lewis Geer, Denis Vakatov

# Build test Fast-CGI application "FASTHELLO"
# NOTES:  - it will be automagically built as a plain CGI application if
#           Fast-CGI libraries are missing on your machine.
#         - also, it auto-detects if it is run as a FastCGI or a plain
#           CGI, and behave appropriately.
#################################

WATCHERS = grichenk

APP = fasthello.fcgi
SRC = helloapp hellores hellocmd

### BEGIN COPIED SETTINGS
LIB = xfcgi xhtml xutil xncbi
LIBS = $(FASTCGI_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

CHECK_CMD =

REQUIRES = unix
