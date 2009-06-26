#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = bam_test
SRC = bam_test

LIB = $(SOBJMGR_LDEP)
LIBS = -Wl,-Bstatic -lalign-access -lvdb -lklib -lkascii -lsraxf -lnucstrstr \
        -lkxf -lkdb -lktrie -lkpt -lkcs -lktxt -lkcont -Wl,-Bdynamic \
        $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

SRA_TOP = $(HOME)/extra/internal/asm-trace
CPPFLAGS = $(ORIG_CPPFLAGS) -I$(SRA_TOP)/itf
LDFLAGS = $(ORIG_LDFLAGS) -L$(SRA_TOP)/bin/lib64 -L$(SRA_TOP)/bin/ilib64
#LDFLAGS = $(ORIG_LDFLAGS) -L$(SRA_TOP)/bin/lib32 -L$(SRA_TOP)/bin/ilib32
