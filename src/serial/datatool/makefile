ANTLR_DIR=../../../external/pccts

include $(ANTLR_DIR)/makefile.mk

# user defined names
CFLAGS=-O +w +w2 -D_DEBUG0 -DNCBI_USE_NEW_HEADERS -I. -I../../../include -I../../../inc $(COTHER)
COTHER = 
OBJ = alexer.o aparser.o parser.o lexer.o type.o value.o module.o main.o

parser: $(OBJ)
	$(LINK) -o parser $(OBJ) $(LFLAGS) -L../../../lib -lxncbi

clean:
	rm -rf *.o SunWS_cache/
