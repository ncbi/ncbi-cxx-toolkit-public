# $Id$
# Author:  Paul Thiessen

# Build application "struct_dp_demo"
#################################

APP = struct_dp_demo

SRC = \
	struct_dp_demo

LIB = \
	xstruct_dp \
	xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(srcdir)/..
