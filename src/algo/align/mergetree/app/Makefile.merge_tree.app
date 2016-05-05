
APP = merge_tree
SRC = merge_tree_app  


LIB = xmergetree xalgoalignutil xalnmgr tables scoremat \
      $(GENBANK_LIBS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

