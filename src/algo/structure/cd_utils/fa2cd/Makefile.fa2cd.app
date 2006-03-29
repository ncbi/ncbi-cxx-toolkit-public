# $Id$
#

# Build application 'fa2cd'
###############################

REQUIRES = objects

ASN_DEP = cdd

APP = fa2cd

SRC = cuFastaToCD

LIB =   xbma_refiner \
        xcd_utils \
        xstruct_util \
        xstruct_dp \
        xblast seqdb blastdb xnetblastcli xnetblast tables \
        xobjread \
        cdd \
        ncbimime \
        cn3d \
        mmdb \
        scoremat \
        seqset $(SEQ_LIBS) \
        pub \
        medline \
        biblio \
        taxon1 \
        general \
        xser \
        xutil \
        xconnect \
        xncbi 


#CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) 
#CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) -D_READFASTA_TEST

#NCBI_C_LIBS = -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lblastcompadj -lncbiobj -lncbi

LIBS = $(ORIG_LIBS)

#LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) 

###  PUT YOUR OWN ADDITIONAL TARGETS (MAKE COMMANDS/RULES) HERE

#install:  fa2cd
#	mv -i fa2cd ~/saikat/fa2cd
