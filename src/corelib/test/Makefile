#################################

IMPORT = .
EXPORT = $(IMPORT)

DEP = \
../include/ncbicgi.hpp \
../include/ncbicgi.inl \
../include/ncbiconf.h \
../include/ncbidbg.hpp \
../include/ncbidiag.hpp \
../include/ncbidiag.inl \
../include/ncbiexpt.hpp \
../include/ncbistd.hpp \
../include/ncbistl.hpp \
../include/ncbistre.hpp \
../include/ncbitype.h

OBJ  = corelib.o
LIBS = -lxncbi
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
LFLAGS    = -L../src -L$(I_LIB)

.SUFFIXES:
.SUFFIXES:	.cpp $(SUFFIXES)

.cpp.o: $(DEP)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@



#################################


$(EXE): $(LIB) $(OBJ)
	$(CXX) $(OBJ) -o $(EXE) $(CXXFLAGS) $(LFLAGS) $(LIBS)


all: $(EXE)

install: $(EXE)
	$(COPY) $(EXE) $(E_BIN)/

clean:
	-rm -r SunWS_cache
	-$(DEL) $(EXE)
	-$(DEL) $(OBJ)
