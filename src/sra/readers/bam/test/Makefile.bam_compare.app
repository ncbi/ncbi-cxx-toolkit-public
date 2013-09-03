# $Id$
# Author:  Eugene Vasilchenko


APP = bam_compare
SRC = bam_compare samtools

REQUIRES = objects Linux Z

SAMTOOLS = $(NCBI)/samtools
SAMTOOLS_INCLUDE = -I$(SAMTOOLS)/include
SAMTOOLS_LIBS = -L$(SAMTOOLS)/lib -lbam

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(SAMTOOLS_INCLUDE)

LIB =   bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil xobjsimple \
        $(OBJMGR_LIBS)
LIBS =  $(SAMTOOLS_LIBS) $(SRA_SDK_SYSLIBS) $(ORIG_LIBS)

CHECK_CMD = bam_compare -q MT:10000-20000 /CHECK_NAME=bam_compare

WATCHERS = vasilche ucko
