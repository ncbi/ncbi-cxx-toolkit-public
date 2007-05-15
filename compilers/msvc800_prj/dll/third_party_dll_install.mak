# $Id$
#################################################################
#
# Main Installation makefile for third-party DLLs'
#
# Author: Viatcheslav Gorelenkov
#
#################################################################



#################################################################
#
# Overridable variables
# These can be modified on the NMAKE command line
#


#
# This is the main path for installation.  It can be overridden
# on the command-line
INSTALL       = .\bin



#################################################################
#
# Do not override any of the variables below on the NMAKE command
# line
#

INTDIR = $(INTDIR:.\=)
ALTDIR = $(INTDIR:VTune_=)


#
# Third-party DLLs' installation path and rules
#
INSTALL_BINPATH          = $(INSTALL)\$(INTDIR)
THIRDPARTY_MAKEFILES_DIR =  .
STAMP_SUFFIX =
!include $(THIRDPARTY_MAKEFILES_DIR)\Makefile.mk



THIRD_PARTY_LIBS = \
				install_fltk       \
				install_berkeleydb \
				install_sqlite     \
				install_sqlite3    \
				install_wxwindows  \
				install_sybase     \
				install_mysql      \
				install_mssql      \
				install_openssl    \
				install_lzo



#
# Main Targets
#
all : dirs \
    $(THIRD_PARTY_LIBS)

clean :
    @echo Removing third-party DLLs' installation...
    @del /S /Q $(INSTALL_BINPATH)\*.dll


###############################################################
#
# Target: Create output directory - may be not present if
# C++ Toolkit was not build yet
#
dirs :
    @echo Creating installation target directory...
    @if not exist $(INSTALL_BINPATH) mkdir $(INSTALL_BINPATH)

