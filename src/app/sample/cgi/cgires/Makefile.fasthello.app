# $Id$
# Authors:  Lewis Geer, Denis Vakatov

# Build test Fast-CGI application "FASTHELLO"
# NOTES:  - it will be automagically built as a plain CGI application if
#           Fast-CGI libraries are missing on your machine.
#         - also, it auto-detects if it is run as a FastCGI or a plain
#           CGI, and behave appropriately.
#################################

APP = fasthello
SRC = helloapp hellores hellocmd
LIB = xhtml xfcgi xncbi

LIBS = $(FASTCGI_LIBS) $(ORIG_LIBS)

CHECK_CMD =

REQUIRES = unix
