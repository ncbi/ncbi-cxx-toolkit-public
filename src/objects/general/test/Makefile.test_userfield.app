REQUIRES = objects C-Toolkit

APP = test_userfield
SRC = test_userfield

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE) 
LIB = general xser xutil xncbi

NCBI_C_LIBS = -lncbiobj $(NCBI_C_ncbi)
LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_LIBS) $(ORIG_LIBS)


WATCHERS = gouriano
