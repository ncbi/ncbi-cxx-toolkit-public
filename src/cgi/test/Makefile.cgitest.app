#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

# Build a test CGI application
#################################

APP = cgitest
SRC = cgitest
LIB = xcgi xncbi

CHECK_CMD = (echo \"test\" | cgitest)
