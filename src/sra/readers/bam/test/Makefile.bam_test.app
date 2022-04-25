#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "sra_test"
#################################

APP = bam_test
SRC = bam_test bam_test_common

LIB =   bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil xobjsimple \
        $(OBJMGR_LIBS)
LIBS =  $(GENBANK_THIRD_PARTY_LIBS) $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -no_index -check_count 100000 /CHECK_NAME=bam_test_no_index0
CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -check_count 100000 /CHECK_NAME=bam_test_no_index1
CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -refseq 20 -refpos 100000 -refwindow 100000 -check_count 15219 /CHECK_NAME=bam_test
CHECK_CMD = bam_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -refseq 20 -refpos 100000 -refwindow 100000 -by-start -check_count 15215 /CHECK_NAME=bam_test2
CHECK_CMD = bam_test -refseq GL000207.1 -refpos 50 -refwindow 100 -check_count 2 /CHECK_NAME=bam_test3
CHECK_CMD = bam_test -refseq GL000207.1 -refpos 50 -refwindow 100 -by-start -check_count 1 /CHECK_NAME=bam_test4
CHECK_CMD = bam_test -refseq GL000207.1 -refwindow 1 -check_count 0 /CHECK_NAME=bam_test_none
CHECK_CMD = bam_test -file 1k.unaligned.bam -check_count 0 /CHECK_NAME=bam_test_none2
CHECK_CMD = bam_test -file header-only.bam -check_count 0 /CHECK_NAME=bam_test_none3
#CHECK_CMD = bam_test -file minimal.bam -check_count 0 /CHECK_NAME=bam_test_none4
CHECK_CMD = bam_test -file 1k.unaligned.bam -refseq GL000207.1 -refwindow 1 -check_count 0 /CHECK_NAME=bam_test_q_none
CHECK_CMD = bam_test -file header-only.bam -refseq GL000207.1 -refwindow 1 -check_count 0 /CHECK_NAME=bam_test_q_none2

CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche ucko
