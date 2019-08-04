# $Id$
# Author:  Eugene Vasilchenko


APP = bam_compare
SRC = bam_compare samtools bam_test_common

REQUIRES = objects Linux Z

SAMTOOLS = $(NCBI)/samtools
SAMTOOLS_INCLUDE = -I$(SAMTOOLS)/include
SAMTOOLS_LIBS = -L$(SAMTOOLS)/lib -lbam

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(SAMTOOLS_INCLUDE)

LIB =   bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil xobjsimple \
        $(OBJMGR_LIBS)
LIBS =  $(GENBANK_THIRD_PARTY_LIBS) $(SAMTOOLS_LIBS) $(SRA_SDK_SYSLIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = bam_compare -raw -q MT:10000-20000 /CHECK_NAME=bam_compare
CHECK_CMD = bam_compare -raw -q GL000207.1:1-2 /CHECK_NAME=bam_compare_none

# this test fails with VDB BAM code due to some empty fields in index
#CHECK_CMD = bam_compare -raw -file minimal.bam /CHECK_NAME=bam_compare_none

# VDB BAM code omits unannotated refseqs so such files are ran in reduced tests mode (no "-raw" option).
# The files are: 1k.unaligned.bam, header-only.bam
CHECK_CMD = bam_compare -file 1k.unaligned.bam /CHECK_NAME=bam_compare_none
CHECK_CMD = bam_compare -file header-only.bam /CHECK_NAME=bam_compare_none
CHECK_CMD = bam_compare -file 1k.unaligned.bam -q GL000207.1:1-2 /CHECK_NAME=bam_compare_q_none
CHECK_CMD = bam_compare -file header-only.bam -q GL000207.1:1-2 /CHECK_NAME=bam_compare_q_none

WATCHERS = vasilche ucko
