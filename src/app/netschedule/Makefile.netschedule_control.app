#################################
# $Id$
#################################

APP = netschedule_control
SRC = netschedule_control



LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
