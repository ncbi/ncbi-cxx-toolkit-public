# $Id$
# Author:  Paul Thiessen

# Build application "struct_util_demo"
#################################

APP = struct_util_demo

SRC = struct_util_demo

LIB =   xstruct_util \
        xstruct_dp \
        ncbimime \
        cdd \
        scoremat \
        cn3d \
        mmdb \
        seqset $(SEQ_LIBS) \
        pub \
        medline \
        biblio \
        general \
        xser \
        xutil \
        xncbi
