# $Id$
# Author:  Josh Cherry

# Build n-mer nucleotide repeat finder app

APP = nmer_repeats
SRC = nmer_repeats
LIB = xalgoseq xregexp regexp xobjread xobjutil $(OBJMGR_LIBS)

REQUIRES = objects dbapi
