# $Id$

# Build test CGI application "sample_cgi"
#################################

#FAST = f
CGI = $(FAST)cgi
CGI_LIBS = $(FAST:f=$(FASTCGI_LIBS))

APP = sample_cgi.$(CGI)
OBJ = sample_cgi
LIB = x$(CGI) xhtml xconnect xncbi connect

LIBS = $(ORIG_LIBS) $(CGI_LIBS) $(NETWORK_LIBS)

