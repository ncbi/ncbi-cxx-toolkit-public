GBENCH = ..\..\..\bin\$(INTDIR)\gbench
DLLBIN = ..\..\..\bin\$(INTDIR)
SRCDIR = ..\..\..\..\..\..\src
THIRDPARTYLIB = ..\..\..\..\..\..\..\lib\$(INTDIR)

COREFILES = \
        $(GBENCH)\bin\gbench-bin.exe \
        $(GBENCH)\bin\gbench_plugin_scan.exe

CORELIBS = \
        $(GBENCH)\bin\ncbi_algo.dll \
        $(GBENCH)\bin\ncbi_bdb.dll \
        $(GBENCH)\bin\ncbi_core.dll \
        $(GBENCH)\bin\ncbi_dbapi.dll \
        $(GBENCH)\bin\ncbi_dbapi_driver.dll \
        $(GBENCH)\bin\dbapi_driver_dblib.dll \
        $(GBENCH)\bin\ncbi_general.dll \
        $(GBENCH)\bin\ncbi_image.dll \
        $(GBENCH)\bin\ncbi_lds.dll \
        $(GBENCH)\bin\ncbi_misc.dll \
        $(GBENCH)\bin\ncbi_pub.dll \
        $(GBENCH)\bin\ncbi_seq.dll \
        $(GBENCH)\bin\ncbi_seqext.dll \
        $(GBENCH)\bin\ncbi_sqlite.dll \
        $(GBENCH)\bin\ncbi_validator.dll \
        $(GBENCH)\bin\ncbi_web.dll \
        $(GBENCH)\bin\ncbi_xloader_cdd.dll \
        $(GBENCH)\bin\ncbi_xloader_lds.dll \
        $(GBENCH)\bin\ncbi_xloader_genbank.dll \
        $(GBENCH)\bin\ncbi_xreader.dll \
        $(GBENCH)\bin\ncbi_xreader_id1.dll \
        $(GBENCH)\bin\ncbi_xreader_pubseqos.dll \
        $(GBENCH)\bin\ncbi_xloader_table.dll \
        $(GBENCH)\bin\ncbi_xloader_trace.dll \
        $(GBENCH)\bin\gui_config.dll \
        $(GBENCH)\bin\gui_core.dll \
        $(GBENCH)\bin\gui_dialogs.dll \
        $(GBENCH)\bin\gui_graph.dll \
        $(GBENCH)\bin\gui_utils.dll \
        $(GBENCH)\bin\gui_widgets.dll \
        $(GBENCH)\bin\gui_widgets_aln.dll \
        $(GBENCH)\bin\gui_widgets_misc.dll \
        $(GBENCH)\bin\gui_widgets_seq.dll

all : dirs \
    $(COREFILES) \
    $(CORELIBS) \
    third-party \
    config-files \
    patterns \
    plugin-scan

dirs :
    @echo Creating directory structure...
    @if not exist $(GBENCH) mkdir $(GBENCH)
    @if not exist $(GBENCH)\bin mkdir $(GBENCH)\bin 
    @if not exist $(GBENCH)\etc mkdir $(GBENCH)\etc 
    @if not exist $(GBENCH)\plugins mkdir $(GBENCH)\plugins 
    @if not exist $(GBENCH)\etc\patterns mkdir $(GBENCH)\etc\patterns 

$(COREFILES) : $(DLLBIN)\$(*B).exe
    @if exist $** echo Updating $(*B).exe...
    @if exist $** copy $** $(GBENCH)\bin > NUL

$(CORELIBS) : $(DLLBIN)\$(*B).dll
    @if exist $** echo Updating $(*B).dll...
    @if exist $(DLLBIN)\$(*B).pdb  copy $(DLLBIN)\$(*B).pdb $(GBENCH)\bin > NUL
    @if exist $(DLLBIN)\$(*B).exp  copy $(DLLBIN)\$(*B).exp $(GBENCH)\bin > NUL
    @if exist $** copy $** $(GBENCH)\bin > NUL

third-party : fltkdll libdb41 sqlite

fltkdll libdb41 sqlite :
    @echo Copying third-party library: $@.dll
    @if exist $(THIRDPARTYLIB)\$@.dll  copy $(THIRDPARTYLIB)\$@.dll $(GBENCH)\bin > NUL
    @if exist $(THIRDPARTYLIB)\$@.pdb  copy $(THIRDPARTYLIB)\$@.pdb $(GBENCH)\bin > NUL
    @if not exist $(GBENCH)\bin\$@.dll copy \\DIZZY\public\msvc7\lib\$(INTDIR)\$@.dll $(GBENCH)\bin > NUL

config-files :
    @echo Copying config files... 
    @copy $(SRCDIR)\gui\gbench\gbench.ini $(GBENCH)\etc > NUL
    @copy $(SRCDIR)\gui\gbench\news.ini   $(GBENCH)\etc > NUL
    @copy $(SRCDIR)\gui\gbench\algo_urls  $(GBENCH)\etc > NUL

patterns :
    @echo Copying patterns...
    @copy $(SRCDIR)\gui\gbench\patterns\kozak.ini $(GBENCH)\etc\patterns > NUL

plugin-scan :
    @echo Creating plugin cache file... 
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\plugins
