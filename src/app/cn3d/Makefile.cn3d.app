# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

REQUIRES = gui ctools

APP = cn3d

OBJ = \
	alignment_manager \
	alignment_set \
	annotate_dialog \
	atom_set \
	block_multiple_alignment \
	bond \
	chemical_graph \
	cdd_annot_dialog \
	cdd_ref_dialog \
	cn3d_blast \
	cn3d_cache \
	cn3d_colors \
	cn3d_main_wxwin \
	cn3d_png \
	cn3d_threader \
	conservation_colorer \
	coord_set \
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

# ...the following two assignments switch to the development configuration:
WXWIN_INCLUDE = $(WX_CPPFLAGS) $(GTK_CFLAGS)
WXWIN_LIBS    = $(WX_LIBS) $(GTK_LIBS)


CPPFLAGS = \
   $(ORIG_CPPFLAGS) \
   -I$(srcdir)/.. \
   $(WXWIN_INCLUDE) \
   $(NCBI_C_INCLUDE)

LIBS = \
   $(ORIG_LIBS) \
   $(WXWIN_LIBS) \
   $(NCBI_C_LIBPATH) \
      -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi
