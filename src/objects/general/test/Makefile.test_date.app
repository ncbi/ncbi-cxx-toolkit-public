APP = test_date
SRC = test_date

LIB     = general xser xutil xncbi
LDFLAGS = $(ORIG_LDFLAGS) $(NCBI_C_LIBPATH)
LIBS    = $(NCBI_C_ncbi) $(ORIG_LIBS)
