# $Id$

# Build test CGI application "cgi_tunnel2grid"
#################################

APP = cgi_tunnel2grid.cgi
SRC = cgi_tunnel2grid

LIB = xgridcgi xconnserv xthrserv xcgi xhtml xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#CHECK_CMD  =
CHECK_COPY = cgi_tunnel2grid_sample.html cgi_tunnel2grid.ini

WATCHERS = sadyrovr
