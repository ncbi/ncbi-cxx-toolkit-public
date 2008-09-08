#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 
APP = update_omssamod
SRC = update_omssamod
#OBJ = unimod

# PRE_LIBS = $(NCBI_C_LIBPATH) .....
#PRE_LIBS = libunimod.a

LIB = omssa unimod blast composition_adjustment tables general xser xutil xncbi seqdb blastdb xconnect $(SOBJMGR_LIBS)
#                                                                         ###
#############################################################################
