#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "sra_test"
#################################

APP = bam_test
SRC = bam_test

LIB =   bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil xobjsimple \
        $(OBJMGR_LIBS)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -refseq 20 /CHECK_NAME=bam_test
CHECK_CMD = bam_test -refseq GL000207.1 -refwindow 1 /CHECK_NAME=bam_test_none
CHECK_CMD = bam_test -file 1k.unaligned.bam /CHECK_NAME=bam_test_none
CHECK_CMD = bam_test -file header-only.bam /CHECK_NAME=bam_test_none
#CHECK_CMD = bam_test -file minimal.bam /CHECK_NAME=bam_test_none
CHECK_CMD = bam_test -file 1k.unaligned.bam -refseq GL000207.1 -refwindow 1 /CHECK_NAME=bam_test_q_none
CHECK_CMD = bam_test -file header-only.bam -refseq GL000207.1 -refwindow 1 /CHECK_NAME=bam_test_q_none

CHECK_REQUIRES = in-house-resources -MSWin -Solaris

WATCHERS = vasilche ucko
