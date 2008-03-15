# $Id$
#################################################################
#
# Main Installation makefile for Genome Workbench
#
# Author: Mike DiCuccio
#
#################################################################



#################################################################
#
# Overridable variables
# These can be modified on the NMAKE command line
#

#
# Tag defining prefixes for a number of paths
MSVC = msvc7

#
# This is the main path for installation.  It can be overridden
# on the command-line
INSTALL       = ..\..\..\bin

#
# What kind of copy to use
#
COPY = copy
#COPY = xcopy


#################################################################
#
# Do not override any of the variables below on the NMAKE command
# line
#

!IF "$(INTDIR)" == ".\DebugDLL"
INTDIR = DebugDLL
!ELSEIF "$(INTDIR)" == ".\ReleaseDLL"
INTDIR = ReleaseDLL
!ENDIF

#
# Alias for the bin directory
#
DLLBIN        = $(INSTALL)\$(INTDIR)

#
# Alias for the genome workbench path
#
GBENCH        = $(DLLBIN)\gbench

#
# Third-party DLLs' installation path and rules
#
INSTALL_BINPATH          = $(GBENCH)\bin
THIRDPARTY_MAKEFILES_DIR = ..\..\..
STAMP_SUFFIX             = _gbench
#!include $(THIRDPARTY_MAKEFILES_DIR)\Makefile.mk
!include $(THIRDPARTY_MAKEFILES_DIR)\..\third_party_install.meta.mk



#
# Alias for the source tree
#
SRCDIR        = ..\..\..\..\..\..\src

#
# Alias for the local installation of third party libs
#
THIRDPARTYLIB = ..\..\..\..\..\..\..\lib\$(INTDIR)


#
# Core Libraries
#
CORELIBS = \
        $(GBENCH)\bin\gui_config.dll            \
        $(GBENCH)\bin\gui_objects.dll           \
        $(GBENCH)\bin\gui_core.dll              \
        $(GBENCH)\bin\gui_dialogs.dll           \
        $(GBENCH)\bin\gui_graph.dll             \
        $(GBENCH)\bin\gui_services.dll          \
        $(GBENCH)\bin\gui_utils.dll             \
        $(GBENCH)\bin\gui_widgets.dll           \
        $(GBENCH)\bin\gui_widgets_aln.dll       \
        $(GBENCH)\bin\gui_widgets_misc.dll      \
        $(GBENCH)\bin\gui_widgets_seq.dll       \
        $(GBENCH)\bin\gui_widgets_snp.dll       \
        $(GBENCH)\bin\ncbi_algo.dll             \
        $(GBENCH)\bin\ncbi_bdb.dll              \
        $(GBENCH)\bin\ncbi_core.dll             \
        $(GBENCH)\bin\ncbi_dbapi.dll            \
        $(GBENCH)\bin\ncbi_dbapi_driver.dll     \
        $(GBENCH)\bin\ncbi_general.dll          \
        $(GBENCH)\bin\ncbi_image.dll            \
        $(GBENCH)\bin\ncbi_lds.dll              \
        $(GBENCH)\bin\ncbi_misc.dll             \
        $(GBENCH)\bin\ncbi_pub.dll              \
        $(GBENCH)\bin\ncbi_seq.dll              \
        $(GBENCH)\bin\ncbi_seqext.dll           \
        $(GBENCH)\bin\ncbi_validator.dll        \
        $(GBENCH)\bin\ncbi_web.dll              \
        $(GBENCH)\bin\ncbi_xcache_bdb.dll       \
        $(GBENCH)\bin\ncbi_xcache_netcache.dll  \
        $(GBENCH)\bin\ncbi_xdbapi_ctlib.dll     \
        $(GBENCH)\bin\ncbi_xdbapi_dblib.dll     \
        $(GBENCH)\bin\ncbi_xdbapi_ftds.dll      \
        $(GBENCH)\bin\ncbi_xdbapi_msdblib.dll   \
        $(GBENCH)\bin\ncbi_xdbapi_mysql.dll     \
        $(GBENCH)\bin\ncbi_xdbapi_odbc.dll      \
        $(GBENCH)\bin\ncbi_xloader_blastdb.dll  \
        $(GBENCH)\bin\ncbi_xloader_cdd.dll      \
        $(GBENCH)\bin\ncbi_xloader_genbank.dll  \
        $(GBENCH)\bin\ncbi_xloader_lds.dll      \
        $(GBENCH)\bin\ncbi_xobjsimple.dll       \
        $(GBENCH)\bin\ncbi_xreader.dll          \
        $(GBENCH)\bin\ncbi_xreader_cache.dll    \
        $(GBENCH)\bin\ncbi_xreader_id1.dll      \
        $(GBENCH)\bin\ncbi_xreader_id2.dll
        
OPTIONAL_LIBS = \
        $(GBENCH)\bin\ncbi_xreader_pubseqos.dll

#
# Plugins
#
PLUGINS = \
        $(GBENCH)\plugins\algo_align.dll        \
        $(GBENCH)\plugins\algo_basic.dll        \
        $(GBENCH)\plugins\algo_cn3d.dll         \
        $(GBENCH)\plugins\algo_external.dll     \
        $(GBENCH)\plugins\algo_gnomon.dll       \
        $(GBENCH)\plugins\algo_init.dll         \
        $(GBENCH)\plugins\algo_linkout.dll      \
        $(GBENCH)\plugins\algo_phylo.dll        \
        $(GBENCH)\plugins\algo_submit.dll       \
        $(GBENCH)\plugins\algo_validator.dll    \
        $(GBENCH)\plugins\algo_web_page.dll     \
        $(GBENCH)\plugins\dload_assn_study.dll  \
        $(GBENCH)\plugins\dload_basic.dll       \
        $(GBENCH)\plugins\dload_table.dll       \
        $(GBENCH)\plugins\view_align.dll        \
        $(GBENCH)\plugins\view_graphic.dll      \
        $(GBENCH)\plugins\view_phylotree.dll    \
        $(GBENCH)\plugins\view_table.dll        \
        $(GBENCH)\plugins\view_text.dll         \
        $(GBENCH)\plugins\view_snp.dll          \
        $(GBENCH)\plugins\view_validator.dll

#
# Resource files
#
RESOURCES = \
        share\gbench\*.png                  \
        \
        etc\algo_urls                       \
        etc\gbench.asn                      \
        etc\gbench_cache_agent.ini          \
        etc\gbench-objmgr.ini               \
        etc\news.ini                        \
        etc\plugin_config.asn               \
        etc\web_pages.ini                   \
        \
        etc\blastdb-spec\blast-dbs.asn      \
        \
        etc\dialogs\feat_edit               \
        etc\dialogs\feat_edit_res           \
        \
        etc\align_scores\aa-rasmol-colors   \
        etc\align_scores\aa-shapely-colors  \
        etc\align_scores\blosum45           \
        etc\align_scores\blosum62           \
        etc\align_scores\blosum80           \
        etc\align_scores\hydropathy         \
        etc\align_scores\membrane           \
        etc\align_scores\na-colors          \
        etc\align_scores\signal             \
        etc\align_scores\size


#
# Third-Party Libraries
# These may depend on the build mode
#
THIRD_PARTY_LIBS = \
        install_fltk       \
        install_berkeleydb \
        install_sqlite     \
        install_msvc

#
# Pattern Files
#
PATTERNS = \
        $(GBENCH)\etc\patterns\kozak.ini


#
#
# Main Targets
#
all : dirs \
    $(CORELIBS) \
    $(OPTIONAL_LIBS) \
    $(THIRD_PARTY_LIBS) \
    $(PLUGINS) \
    $(INTERNAL_PLUGINS) \
    $(RESOURCES) \
    EXTRA_RESOURCES \
    $(PATTERNS) \
    $(GBENCH)/plugins/plugin-cache

clean :
    @echo Removing Genome Workbench Installation...
    -@rmdir /S /Q $(GBENCH)


###############################################################
#
# Target: Create our directory structure
#
dirs :
    @echo Creating directory structure...
    @if not exist $(GBENCH) mkdir $(GBENCH)
    @if not exist $(GBENCH)\bin mkdir $(GBENCH)\bin
    @if not exist $(GBENCH)\etc mkdir $(GBENCH)\etc
    @if not exist $(GBENCH)\plugins mkdir $(GBENCH)\plugins
    @if not exist $(GBENCH)\etc\patterns mkdir $(GBENCH)\etc\patterns
    @if not exist $(GBENCH)\share mkdir $(GBENCH)\share

###############################################################
#
# Target: Copy the core libraries
#
$(CORELIBS) : $(DLLBIN)\$(*B).dll
    @if exist $** echo Updating $(*B).dll...
    @if exist $(DLLBIN)\$(*B).pdb  $(COPY) $(DLLBIN)\$(*B).pdb $(GBENCH)\bin > NUL
    @if exist $** $(COPY) $** $(GBENCH)\bin > NUL

###############################################################
#
# Target: Copy the optional libraries
#
$(OPTIONAL_LIBS) :
    @if exist $(DLLBIN)\$(*B).dll echo Updating $(*B).dll...
    @if exist $(DLLBIN)\$(*B).pdb $(COPY) $(DLLBIN)\$(*B).pdb $(GBENCH)\bin > NUL
    @if exist $(DLLBIN)\$(*B).dll $(COPY) $(DLLBIN)\$(*B).dll $(GBENCH)\bin > NUL

###############################################################
#
# Target: Copy the plugins
#
$(PLUGINS) : $(DLLBIN)\$(*B).dll
    @if exist $** echo Updating $(*B).dll...
    @if exist $** $(COPY) $** $(GBENCH)\plugins > NUL
    @if exist $(DLLBIN)\$(*B).pdb $(COPY) $(DLLBIN)\$(*B).pdb $(GBENCH)\plugins > NUL

###############################################################
#
# Target: Copy the resources
#
$(RESOURCES) : $(SRCDIR)\gui\res\$@
    @if not exist $(GBENCH)\$(*D) mkdir $(GBENCH)\$(*D)
    @if exist $(SRCDIR)\gui\res\$@ echo Updating $@...
    @if exist $(SRCDIR)\gui\res\$@ $(COPY) $(SRCDIR)\gui\res\$@ $(GBENCH)\$@ > NUL

EXTRA_RESOURCES : $(SRCDIR)\objects\seqloc\accguide.txt
    @if not exist $(GBENCH)\etc mkdir $(GBENCH)\etc 
    $(COPY) $(SRCDIR)\objects\seqloc\accguide.txt $(GBENCH)\etc

###############################################################
#
# Target: Copy the pattern files
#
$(PATTERNS) : $(SRCDIR)\gui\res\etc\patterns\$(*B).ini
    @if exist $** echo Updating $(*B).ini...
    @if exist $** $(COPY) $** $(GBENCH)\etc\patterns > NUL


###############################################################
#
# Target: Update the plugin cache, if necessary
#
$(GBENCH)/plugins/plugin-cache : $(PLUGINS) $(GBENCH)\bin\gbench_plugin_scan.exe
    @echo Creating plugin cache file...
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\plugins
