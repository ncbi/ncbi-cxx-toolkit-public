#include ../../makefile.in

CCC = CC -xildoff

SUFFIXES += .cpp
.SUFFIXES: $(SUFFIXES)

CFLAGS = -g -I/netopt/stlport/3.01 -I../include -I../../corelib/include -DNCBI_SGI_STL_PORT

#CFLAGS = $(iLOCALINCPATH) $(iINCPATH) $(iCFLAGS)
#LDFLAGS = $(iLDFLAGS) -L../../libs $(iLOCALLIBS) $(iLIBS) -lm
#LDFLAGS = $(iLDFLAGS) \
#-L../../libs \
#$(iLOCALLIBS) \
#$(iPUBMEDLIB) \
#$(iLIBS) \
#$(iCPLUSSHLIB) \
#-L$(NCBI_LIB) -L$(SYBASE)/lib \
#-lm -ldl \
#-lpmapi -lpmncbi -lpmdisp -lmisc \
#-lm -ldl \
#-lpmapi -lpmncbi -lpmdisp \
#-lncbimla \
#-lncbimsc1 \
#-lncbimmdb \
#-lncbiacc \
#-lncbicdr \
#-lncbitax1 \
#-lncbiobj \
#-lncbiid0 \
#-lncbi \
#-lnetcli \
#-lnlmzip \
#-lserver \
#-lpment \
#-lmisc \
#-Bstatic -lsybdb -Bdynamic \
#-lresolv  -lsocket  -lrpcsvc  -lnsl \
#-lgen  -lm -R/usr/lib -R/netopt/SUNWspro/lib \
#-L/netopt/SUNWspro4/lib -lC \
#-lm \
#-ldl

EXE = frontpage

OBJ =  frontpage.o page.o html.o node.o components.o

.cpp.o:
	$(CCC) $(CFLAGS) -c $<

all: $(EXE)

$(EXE): $(OBJ) 
	$(CCC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

#	$(CC) $(OBJ) $(LDFLAGS) -o $@


clean:
	rm -f *.o $(EXE) core *~

install: $(EXE)
	/usr/ucb/install -c $(EXE) $(BINDEST)

debug: $(EXE)

purify:
	rm -f $(EXE)
	$(MAKE) debug "CCC=purify CC"

.KEEP_STATE:
