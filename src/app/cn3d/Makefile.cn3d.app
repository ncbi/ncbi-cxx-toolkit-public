# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

REQUIRES = objects wxWidgets OpenGL PNG

APP = cn3d

SRC =	aaa_dummy_pch \
	alignment_manager \
	alignment_set \
	animation_controls \
	annotate_dialog \
	atom_set \
	block_multiple_alignment \
	bond \
	cdd_annot_dialog \
	cdd_book_ref_dialog \
	cdd_ref_dialog \
	cdd_splash_dialog \
	chemical_graph \
	cn3d_app \
	cn3d_ba_interface \
	cn3d_blast \
	cn3d_cache \
	cn3d_colors \
	cn3d_glcanvas \
	cn3d_png \
	cn3d_pssm \
	cn3d_refiner_interface \
	cn3d_threader \
	cn3d_tools \
	command_processor \
	conservation_colorer \
	coord_set \
	data_manager \
	dist_select_dialog \
	file_messaging \
	messenger \
	molecule \
	molecule_identifier \
	multitext_dialog \
	object_3d \
	opengl_renderer \
	pattern_dialog \
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
	structure_window \
	style_dialog \
	style_manager \
	taxonomy_tree \
	update_viewer \
	update_viewer_window \
	vector_math \
	viewer_base \
	viewer_window_base \
	wx_tools

LIB =	xcd_utils xbma_refiner xstruct_util xstruct_dp xstruct_thread \
	xblast xalgoblastdbindex composition_adjustment seqdb xobjread tables \
	id1cli id1 cdd ncbimime cn3d mmdb scoremat seqset seq seqcode sequtil \
	pub medline biblio general taxon1 blastdb xnetblast \
	xregexp xser xutil xconnect xncbi $(Z_LIB) $(PCRE_LIB)

WXWIDGETS_INSTALL = /home/thiessen/wxWidgets-install
ifeq ($(DEBUG_SFX),Debug)
	WXWIDGETS_CONFIG_DEBUG = --debug=yes
else
	WXWIDGETS_CONFIG_DEBUG = --debug=no
endif
WXWIDGETS_CONFIG_FLAGS = $(WXWIDGETS_CONFIG_DEBUG) --static=yes --unicode=no
WXWIDGETS_INCLUDE := $(shell $(WXWIDGETS_INSTALL)/bin/wx-config $(WXWIDGETS_CONFIG_FLAGS) --cxxflags)
WXWIDGETS_LIBS := $(shell $(WXWIDGETS_INSTALL)/bin/wx-config $(WXWIDGETS_CONFIG_FLAGS) --libs std,gl)

CPPFLAGS = \
	$(ORIG_CPPFLAGS) \
	$(WXWIDGETS_INCLUDE) \
	$(Z_INCLUDE) $(PNG_INCLUDE) $(PCRE_INCLUDE)

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS = $(FAST_LDFLAGS)

LIBS = \
	$(WXWIDGETS_LIBS) \
	$(Z_LIBS) $(PNG_LIBS) $(PCRE_LIBS) \
	$(ORIG_LIBS)


# for distribution on linux/gcc, do:
#   gcc  ....  -lpthread -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
