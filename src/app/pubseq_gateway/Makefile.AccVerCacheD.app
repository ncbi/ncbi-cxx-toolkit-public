#
#
###  BASIC PROJECT SETTINGS
APP = AccVerCacheD
SRC = AccVerCacheD AccVerCacheDB AccVerCacheStorage Lib/Key Lib/CassBlobOp

#MY_INCLUDE=./myinclude
#MY_LIBS=-L./libh2o -lh2o -L./mylib -lssl -lcrypto -luv
# OBJ =

#PRE_LIBS = $(MY_LIBS) 

LOCAL_CPPFLAGS = -I. $(LMDB_INCLUDE) -I./external -I./external/include

LIB  = DdRpc h2o ssl crypto idcassandra xncbi connect IdLogUtil
LIBS = $(LMDB_LIBS) -L./external/lib/linux/$(MODE) -L./external/lib64 -L./external/lib -lcassandra_static  -luv-static $(DL_LIBS)
#$(NETWORK_LIBS)  $(ORIG_LIBS) 

#PROFILECXX=-g -O0
#PROFILEL=
#PROFILECXX = -pg -g -O2
#PROFILEL = -pg

#LOCAL_CPPFLAGS = $(PROFILECXX) -I. -I/home/dmitrie1/pilot/build/$(MODE)/include
#LOCAL_CPPFLAGS = $(PROFILECXX)
# -I./libuv/include
#-I$(TOOL_LIB)

#LOCAL_LDFLAGS = $(PROFILEL) -L. -L/home/dmitrie1/pilot/build/$(MODE)/lib
#LOCAL_LDFLAGS = $(PROFILEL)
#-L$(TOOL_LIB)

#LOCAL_CPPFLAGS += -pedantic -Werror

LOCAL_CPPFLAGS += -g 
LOCAL_LDFLAGS += -g

ifeq "$(MODE)" "Debug"
#EXTRA=-fno-omit-frame-pointer -fsanitize=address
#EXTRA=-fno-omit-frame-pointer -fsanitize=leak
LOCAL_CPPFLAGS += -O0 $(EXTRA)
LOCAL_LDFLAGS += $(EXTRA)
endif

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)
