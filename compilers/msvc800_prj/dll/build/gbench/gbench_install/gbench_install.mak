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
!include $(THIRDPARTY_MAKEFILES_DIR)\Makefile.mk


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
        $(GBENCH)\bin\gui_core.dll              \
        $(GBENCH)\bin\gui_dialogs.dll           \
        $(GBENCH)\bin\gui_graph.dll             \
        $(GBENCH)\bin\gui_utils.dll             \
        $(GBENCH)\bin\gui_services.dll          \
        $(GBENCH)\bin\gui_widgets.dll           \
        $(GBENCH)\bin\gui_widgets_aln.dll       \
        $(GBENCH)\bin\gui_widgets_misc.dll      \
        $(GBENCH)\bin\gui_widgets_seq.dll       \
        $(GBENCH)\bin\ncbi_algo.dll             \
        $(GBENCH)\bin\ncbi_bdb.dll              \
        $(GBENCH)\bin\ncbi_core.dll             \
        $(GBENCH)\bin\ncbi_dbapi.dll            \
        $(GBENCH)\bin\ncbi_dbapi_driver.dll     \
        $(GBENCH)\bin\ncbi_general.dll          \
        $(GBENCH)\bin\ncbi_image.dll            \
        $(GBENCH)\bin\ncbi_lds.dll              \
        $(GBENCH)\bin\ncbi_misc.dll             \
        $(GBENCH)\bin\ncbi_mmdb.dll             \
        $(GBENCH)\bin\ncbi_pub.dll              \
        $(GBENCH)\bin\ncbi_seq.dll              \
        $(GBENCH)\bin\ncbi_seqext.dll           \
        $(GBENCH)\bin\ncbi_sqlite.dll           \
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
        $(GBENCH)\bin\ncbi_xloader_cdd.dll      \
        $(GBENCH)\bin\ncbi_xloader_genbank.dll  \
        $(GBENCH)\bin\ncbi_xloader_lds.dll      \
        $(GBENCH)\bin\ncbi_xloader_table.dll    \
        $(GBENCH)\bin\ncbi_xobjsimple.dll       \
        $(GBENCH)\bin\ncbi_xreader.dll          \
        $(GBENCH)\bin\ncbi_xreader_cache.dll    \
        $(GBENCH)\bin\ncbi_xreader_id1.dll      \
        $(GBENCH)\bin\ncbi_xreader_id2.dll      \
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
        $(GBENCH)\plugins\algo_validator.dll    \
        $(GBENCH)\plugins\algo_web_page.dll     \
        $(GBENCH)\plugins\dload_basic.dll       \
        $(GBENCH)\plugins\dload_table.dll       \
        $(GBENCH)\plugins\view_align.dll        \
        $(GBENCH)\plugins\view_graphic.dll      \
        $(GBENCH)\plugins\view_phylotree.dll    \
        $(GBENCH)\plugins\view_table.dll        \
        $(GBENCH)\plugins\view_taxplot.dll      \
        $(GBENCH)\plugins\view_text.dll         \
        $(GBENCH)\plugins\view_validator.dll

INTERNAL_PLUGINS = \
        ncbi_gbench_internal
        
#
# Resource files
#
RESOURCES = \
        share\gbench\about.png        	    \
        share\gbench\annot_folder.png       \
        share\gbench\annot_item.png         \
        share\gbench\attachment_item.png    \
        share\gbench\back.png               \
        share\gbench\broadcast.png          \
        share\gbench\broadcast_options.png  \
        share\gbench\check.png              \
        share\gbench\close_container.png    \
        share\gbench\doc_item.png           \
        share\gbench\doc_item_disabled.png  \
        share\gbench\export.png        	    \
        share\gbench\folder.png             \
        share\gbench\forward.png            \
        share\gbench\gbench_about.png       \
        share\gbench\help.png        	    \
        share\gbench\history_folder.png     \
        share\gbench\import.png        	    \
        share\gbench\mouse_mode_def.png        \
        share\gbench\mouse_mode_pan.png        \
        share\gbench\mouse_mode_zoom_in.png    \
        share\gbench\mouse_mode_zoom_out.png   \
        share\gbench\mouse_mode_zoom_rect.png  \
        share\gbench\project_item.png       \
        share\gbench\radio.png              \
        share\gbench\search.png        	    \
        share\gbench\splash.png             \
        share\gbench\splitter_2x2.png       \
        share\gbench\splitter_horz.png      \
        share\gbench\splitter_vert.png      \
        share\gbench\tab_control.png        \
        share\gbench\tool.png        	    \
        share\gbench\view_item.png          \
        share\gbench\viewer_item.png        \
        share\gbench\wm_menu.png        \
        share\gbench\wm_minimize.png        \
        share\gbench\wm_maximize.png        \
        share\gbench\wm_restore.png        \
        share\gbench\wm_close.png           \
        share\gbench\workspace_item.png     \
        share\gbench\zoom_all.png           \
        share\gbench\zoom_in.png            \
        share\gbench\zoom_out.png           \
        share\gbench\zoom_sel.png           \
        share\gbench\zoom_seq.png           \
        share\gbench\insp_table_mode.png    \
        share\gbench\insp_brief_text_mode.png           \
        share\gbench\insp_text_mode.png     \
        \
        etc\algo_urls                       \
        etc\gbench.asn                      \
        etc\news.ini                        \
        etc\plugin_config.asn               \
        etc\web_pages.ini                   \
        etc\gbench_cache_agent.ini          \
        \
        etc\dialogs\feat_edit               \
        \
        etc\align_scores\aa-shapely-colors  \
        etc\align_scores\aa-rasmol-colors   \
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
    $(THIRD_PARTY_LIBS) \
    $(PLUGINS) \
    $(INTERNAL_PLUGINS) \
    $(RESOURCES) \
    $(PATTERNS) \
    $(GBENCH)/plugins/plugin-cache

clean :
    @echo Removing Genome Workbench Installation...
    -@rmdir /S /Q $(GBENCH)
    -@del $(GBENCH)\..\..\..\$(INTDIR)\fltk_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\berkeleydb_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\sqlite_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\msvc_gbench.installed


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
# Target: Copy the plugins
#
$(PLUGINS) : $(DLLBIN)\$(*B).dll
    @if exist $** echo Updating $(*B).dll...
    @if exist $** $(COPY) $** $(GBENCH)\plugins > NUL
    @if exist $(DLLBIN)\$(*B).pdb $(COPY) $(DLLBIN)\$(*B).pdb $(GBENCH)\plugins > NUL

###############################################################
#
# Target: Install the standard internal plugins
#
ncbi_gbench_internal :
    @echo Installing NCBI internal plugin...
    @if not exist $(GBENCH)\extra\ncbi mkdir $(GBENCH)\extra\ncbi
    @if exist $(DLLBIN)\ncbi_gbench_internal.dll copy $(DLLBIN)\ncbi_gbench_internal.dll $(GBENCH)\extra\ncbi\ncbi_gbench_internal.dll
    @if exist $(DLLBIN)\ncbi_gbench_internal.pdb copy $(DLLBIN)\ncbi_gbench_internal.pdb $(GBENCH)\extra\ncbi\ncbi_gbench_internal.pdb
    @if exist $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-config.asn copy $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-config.asn $(GBENCH)\extra\ncbi\ncbi-config.asn
    @if exist $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-win32-config.asn copy $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-win32-config.asn $(GBENCH)\extra\ncbi\ncbi-win32-config.asn
    @if exist $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-unix-config.asn copy $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-unix-config.asn $(GBENCH)\extra\ncbi\ncbi-unix-config.asn
    @if exist $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-macos-config.asn copy $(SRCDIR)\internal\gbench\plugins\ncbi\ncbi-macos-config.asn $(GBENCH)\extra\ncbi\ncbi-macos-config.asn
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\extra\ncbi

###############################################################
#
# Target: Copy the resources
#
$(RESOURCES) : $(SRCDIR)\gui\res\$@
    @if not exist $(GBENCH)\$(*D) mkdir $(GBENCH)\$(*D)
    @if exist $(SRCDIR)\gui\res\$@ echo Updating $@...
    @if exist $(SRCDIR)\gui\res\$@ $(COPY) $(SRCDIR)\gui\res\$@ $(GBENCH)\$@ > NUL

###############################################################
#
# Target: Copy the pattern files
#
$(PATTERNS) : $(SRCDIR)\gui\gbench\patterns\$(*B).ini
    @if exist $** echo Updating $(*B).ini...
    @if exist $** $(COPY) $** $(GBENCH)\etc\patterns > NUL


###############################################################
#
# Target: Update the plugin cache, if necessary
#
$(GBENCH)/plugins/plugin-cache : $(PLUGINS) $(GBENCH)\bin\gbench_plugin_scan.exe
    @echo Creating plugin cache file...
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\plugins
