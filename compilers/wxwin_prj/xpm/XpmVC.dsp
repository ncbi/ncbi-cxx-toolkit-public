# Microsoft Developer Studio Project File - Name="xpm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=xpm - Win32 ReleaseMT
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xpm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xpm.mak" CFG="xpm - Win32 ReleaseMT"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xpm - Win32 ReleaseMT" (based on "Win32 (x86) Static Library")
!MESSAGE "xpm - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "xpm - Win32 DebugMT" (based on "Win32 (x86) Static Library")
!MESSAGE "xpm - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xpm - Win32 ReleaseMT"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMT"
# PROP BASE Intermediate_Dir "ReleaseMT"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMT"
# PROP Intermediate_Dir "ReleaseMT"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\ReleaseMT\xpm.lib"

!ELSEIF  "$(CFG)" == "xpm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Release\xpm.lib"

!ELSEIF  "$(CFG)" == "xpm - Win32 DebugMT"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugMT"
# PROP BASE Intermediate_Dir "DebugMT"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugMT"
# PROP Intermediate_Dir "DebugMT"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /ZI /Z7 /Od /D "__WXDEBUG__=1" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\DebugMT\xpm.lib"

!ELSEIF  "$(CFG)" == "xpm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MLd /W3 /Gm /GX /Zi /ZI /Z7 /Od /D "__WXDEBUG__=1" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "__WINDOWS__" /D "__WXMSW__" /D "__WIN95__" /D "__WIN32__" /D WINVER=0x0400 /D "STRICT" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:".\..\..\Debug\xpm.lib"

!ENDIF 

# Begin Target

# Name "xpm - Win32 ReleaseMT"
# Name "xpm - Win32 Release"
# Name "xpm - Win32 DebugMT"
# Name "xpm - Win32 Debug"

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\..\..\src\xpm\attrib.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\crbuffri.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\crdatfri.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\create.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\crifrbuf.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\crifrdat.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\dataxpm.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\hashtab.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\imagexpm.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\info.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\misc.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\parse.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\rdftodat.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\rdftoi.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\rgb.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\scan.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\simx.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\wrffrdat.c
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\wrffri.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\..\..\src\xpm\rgbtab.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\simx.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\xpm.h
# End Source File
# Begin Source File

SOURCE=.\..\..\src\xpm\xpmi.h
# End Source File
# End Group
# End Target
# End Project
