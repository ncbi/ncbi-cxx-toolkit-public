#################################

IMPORT = .
EXPORT = $(IMPORT)

OBJ  = corelib.o
LIBS = -lxncbi
LIBS_DEP = ../src/libxncbi.a
EXE  = corelib


#################################

COPY   = cp -p
DEL    = rm -f

I_INCLUDE = $(IMPORT)/include
I_LIB     = $(IMPORT)/lib
I_BIN     = $(IMPORT)/bin

E_INCLUDE = $(EXPORT)/include
E_LIB     = $(EXPORT)/lib
E_BIN     = $(EXPORT)/bin

CXX       = /netopt/SUNWspro6/bin/CC
CPPFLAGS  = -I../include -I$(I_INCLUDE) -D_DEBUG
# -DNCBI_USE_NEW_IOSTREAM
CXXFLAGS  = +w2 -g
LFLAGS    = -xildoff -L../src -L$(I_LIB)

.SUFFIXES:
.SUFFIXES:  .o .c .h .hpp .cpp

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@

.KEEP_STATE:

#################################


$(EXE): $(LIBS_DEP) $(OBJ)
	$(CXX) $(OBJ) -o $(EXE) $(CXXFLAGS) $(LFLAGS) $(LIBS)


all: $(EXE)

install: $(EXE)
	$(COPY) $(EXE) $(E_BIN)/

clean:
	-rm .make.state
	-rm -r SunWS_cache
	-$(DEL) $(EXE)
	-$(DEL) $(OBJ)
