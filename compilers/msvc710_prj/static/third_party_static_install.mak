# $Id$
#################################################################


PTB_GENERATED = $(INTDIR)\Makefile.third_party.mk
META_MAKE = ..\third_party_install.meta.mk


!IF EXIST($(PTB_GENERATED))
!INCLUDE $(PTB_GENERATED)
!ELSE
!ERROR  $(PTB_GENERATED)  not found
!ENDIF

!IF EXIST($(META_MAKE))
!INCLUDE $(META_MAKE)
!ELSE
!ERROR  $(META_MAKE)  not found
!ENDIF

