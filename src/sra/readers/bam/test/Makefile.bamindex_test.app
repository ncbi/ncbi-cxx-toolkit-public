#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "bamindex_test"
#################################

APP = bamindex_test
SRC = bamindex_test bam_test_common

LIB =   bamread $(BAM_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil xobjsimple \
        $(OBJMGR_LIBS)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_CMD = bamindex_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -index_type bai -q 20 -overlap -limit_count 1 /CHECK_NAME=bamindex_test_bai1
CHECK_CMD = bamindex_test -file NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam -index_type csi -q 20 -overlap -limit_count 1 /CHECK_NAME=bamindex_test_csi1
CHECK_CMD = bamindex_test -file hs108_sra.fil_sort.chr1.bam -index_type bai -q NC_000001.11 -overlap -limit_count 1 /CHECK_NAME=bamindex_test_bai2
CHECK_CMD = bamindex_test -file hs108_sra.fil_sort.chr1.bam -index_type csi -q NC_000001.11 -overlap -limit_count 1 /CHECK_NAME=bamindex_test_csi2

# The following are very slow tests:
#CHECK_CMD = bamindex_test -file NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam -index_type bai -q Y -overlap -limit_count 1 /CHECK_NAME=bamindex_test_bai3
#CHECK_CMD = bamindex_test -file NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam -index_type csi -q Y -overlap -limit_count 1 /CHECK_NAME=bamindex_test_csi3
#CHECK_CMD = bamindex_test -file NA10851.SLX.maq.SRP000031.2009_08.bam -index_type bai -q Y -overlap -limit_count 1 /CHECK_NAME=bamindex_test_bai4
#CHECK_CMD = bamindex_test -file NA10851.SLX.maq.SRP000031.2009_08.bam -index_type csi -q Y -overlap -limit_count 1 /CHECK_NAME=bamindex_test_csi4

# smaller samples from the above big files
CHECK_CMD = bamindex_test -file NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam -index_type bai -q Y,hs37d5 -overstart -limit_count 1 /CHECK_NAME=bamindex_test_bai3
CHECK_CMD = bamindex_test -file NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam -index_type csi -q Y,hs37d5 -overstart -limit_count 1 /CHECK_NAME=bamindex_test_csi3
CHECK_CMD = bamindex_test -file NA10851.SLX.maq.SRP000031.2009_08.bam -index_type bai -q Y,NT_113965 -overstart -limit_count 1 /CHECK_NAME=bamindex_test_bai4
CHECK_CMD = bamindex_test -file NA10851.SLX.maq.SRP000031.2009_08.bam -index_type csi -q Y,NT_113965 -overstart -limit_count 1 /CHECK_NAME=bamindex_test_csi4

WATCHERS = vasilche
