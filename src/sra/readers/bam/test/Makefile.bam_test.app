#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "sra_test"
#################################

APP = bam_test
SRC = bam_test

LIB =   bamread xobjreadex submit xobjutil xobjsimple $(OBJMGR_LIBS) $(BAM_LIBS)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -refseq 20 /CHECK_NAME=bam_test
CHECK_REQUIRES = -AIX -BSD -MSWin -Solaris

WATCHERS = vasilche ucko
