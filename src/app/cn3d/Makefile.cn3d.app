# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

REQUIRES = gui ctools

APP = cn3d

SRC = \
	alignment_manager \
	alignment_set \
	annotate_dialog \
	atom_set \
	block_multiple_alignment \
	bond \
	chemical_graph \
	cdd_annot_dialog \
	cdd_ref_dialog \
	cdd_splash_dialog \
	cn3d_ba_interface \
	cn3d_blast \
	cn3d_blockalign \
	cn3d_cache \
	cn3d_colors \
	cn3d_main_wxwin \
	cn3d_png \
	cn3d_threader \
	conservation_colorer \
	coord_set \
	data_manager \
	messenger \
	molecule \
	molecule_identifier \
	multitext_dialog \
	object_3d \
	opengl_renderer \
	periodic_table \
	preferences_dialog \
	progress_meter \
	residue \
	sequence_display \
	sequence_set \
	sequence_viewer \
	sequence_viewer_widget \
	sequence_viewer_window \
	show_hide_dialog \
	show_hide_manager \
	structure_base \
	structure_set \
	style_dialog \
	style_manager \
	update_viewer \
	update_viewer_window \
	vector_math \
	viewer_base \
	viewer_window_base \
	wx_tools

LIB = \
	cdd ncbimime cn3d mmdb1 mmdb2 mmdb3 seqset \
	$(SEQ_LIBS) pub medline	biblio general \
	xser xutil xctools xconnect xncbi


# Development configuration
WX_CONFIG = $(HOME)/Programs/wxWindows/install/bin/wxgtk$(D_SFX)-2.3-config
WX_CPPFLAGS = $(shell $(WX_CONFIG) --cppflags)
WX_LIBS = $(shell $(WX_CONFIG) --libs) $(shell $(WX_CONFIG) --gl-libs)
GTK_CONFIG = $(HOME)/Programs/GTK-1.2-$(DEBUG_SFX)/install/bin/gtk-config
GTK_CFLAGS = $(shell $(GTK_CONFIG) --cflags)
GTK_LIBS = $(shell $(GTK_CONFIG) --libs)

# platform-specific libs
CURRENT_PLATFORM = $(shell uname)
EXTRA_LIBS =
ifeq ($(CURRENT_PLATFORM), SunOS)
	EXTRA_LIBS = -lposix4
endif
ifeq ($(CURRENT_PLATFORM), Linux)
        EXTRA_LIBS = \
		-Wl,-Bstatic -lpng -lz -ljpeg -ltiff -Wl,-Bdynamic \
		-lpthread
endif

# ...the following two assignments switch to the development configuration:
WXWIN_INCLUDE = $(WX_CPPFLAGS) $(GTK_CFLAGS)
WXWIN_LIBS    = $(WX_LIBS) $(GTK_LIBS) $(EXTRA_LIBS)

CPPFLAGS = \
	$(ORIG_CPPFLAGS) \
	-I$(srcdir)/.. \
	$(WXWIN_INCLUDE) \
	$(NCBI_C_INCLUDE)

LIBS = \
	$(WXWIN_LIBS) \
	$(NCBI_C_LIBPATH) \
		-lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi \
	$(ORIG_LIBS)


# for distribution on linux/gcc, the following command line works:
#/usr/bin/gcc -O  alignment_manager.o alignment_set.o annotate_dialog.o atom_set.o block_multiple_alignment.o bond.o chemical_graph.o cdd_annot_dialog.o cdd_ref_dialog.o cdd_splash_dialog.o cn3d_blast.o cn3d_cache.o cn3d_colors.o cn3d_main_wxwin.o cn3d_png.o cn3d_threader.o conservation_colorer.o coord_set.o data_manager.o messenger.o molecule.o molecule_identifier.o multitext_dialog.o object_3d.o opengl_renderer.o periodic_table.o preferences_dialog.o progress_meter.o residue.o sequence_display.o sequence_set.o sequence_viewer.o sequence_viewer_widget.o sequence_viewer_window.o show_hide_dialog.o show_hide_manager.o structure_base.o structure_set.o style_dialog.o style_manager.o update_viewer.o update_viewer_window.o vector_math.o viewer_base.o viewer_window_base.o wx_tools.o    -L/home/paul/Programs/c++/GCC-Release/lib -lcdd -lncbimime -lcn3d -lmmdb1 -lmmdb2 -lmmdb3 -lseqset -lseqfeat -lseq -lseqalign -lseqblock -lseqfeat -lseq -lseqloc -lseqres -lseqcode -lpub -lmedline -lbiblio -lgeneral -lxser -lxutil -lxctools -lxconnect -lxncbi -L/usr/local/src/wxWindows/install/lib -lwx_gtk-2.3 -L/usr/local/lib -lwx_gtk_gl-2.3 -lGL -lGLU -L/usr/lib -L/usr/X11R6/lib -lgdk -lgtk  -lgmodule -lglib -ldl -lXi -lXext -lX11 -lm -L/home/paul/Programs/ncbi/lib -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi -lm -lpthread -o cn3d -Wl,-Bstatic -ljpeg -ltiff -lpng -lz -lstdc++ -Wl,-Bdynamic
