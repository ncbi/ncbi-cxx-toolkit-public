#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "feat_import"
#################################

APP			=	feat_import
SRC			=	feat_import
LIB			=	xobjimport seqset $(SEQ_LIBS) pub medline biblio \
				general xser xconnect xutil xncbi
LIBS		=	$(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS	= ludwigf

REQUIRES	= objects
