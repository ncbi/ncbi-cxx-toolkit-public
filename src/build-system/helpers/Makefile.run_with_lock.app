# $Id$
APP = run_with_lock
SRC = run_with_lock

CC          = $(CC_FOR_BUILD)
CC_WRAPPER  =
CPP         = $(CPP_FOR_BUILD)
CFLAGS      = $(CFLAGS_FOR_BUILD)
CPPFLAGS    = $(CPPFLAGS_FOR_BUILD)
LDFLAGS     = $(LDFLAGS_FOR_BUILD)
LIBS        = $(C_LIBS)
LINK        = $(CC)
LINK_WRAPPER =
APP_LDFLAGS = 

APP_OR_NULL = app
REQUIRES    = unix -XCODE
WATCHERS    = ucko
