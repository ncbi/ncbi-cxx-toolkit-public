# $Id$

#
# Rules for installation of third-party libraries.
# Uses definition provided by project tree builder configure.
#

!include $(THIRDPARTY_MAKEFILES_DIR)\$(INTDIR)\Makefile.third_party.mk

install_fltk:
	@echo Coping FLTK DLLs
	copy /Y $(FLTK_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_berkeleydb:
	@echo Coping BerkeleyDB DLLs
	copy /Y $(BERKELEYDB_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)

install_sqlite:
	@echo Coping SQLITE DLLs
	copy /Y $(SQLITE_BINPATH)\$(INTDIR)\*.dll $(INSTALL_BINPATH)
