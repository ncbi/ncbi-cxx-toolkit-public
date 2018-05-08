# $Id$

APP = deployable_cgi.cgi
SRC = deployable_cgi

LIB  = xcgi xhtml xconnect xutil xncbi

CHECK_COPY = test_deployable_cgi.sh deployable_cgi.html deployable_cgi.ini
CHECK_CMD  = test_deployable_cgi.sh

WATCHERS = fukanchi
