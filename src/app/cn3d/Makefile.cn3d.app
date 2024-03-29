# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

WATCHERS = lanczyck,dzhang

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
	viewer_window_base

CPPFLAGS = $(ORIG_CPPFLAGS) \
	$(WXWIDGETS_INCLUDE) \
	$(Z_INCLUDE) $(PNG_INCLUDE) $(PCRE_INCLUDE)

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS = $(FAST_LDFLAGS)

LIB =   wx_tools xcd_utils xstruct_util xstruct_dp xstruct_thread \
        xblast xalgowinmask xalgodustmask xalgoblastdbindex composition_adjustment seqmasks_io seqdb $(OBJREAD_LIBS) \
	xalnmgr tables xobjutil id1cli id1 ncbimime cdd cn3d mmdb blast_services xnetblastcli xnetblast scoremat \
	utrtprof taxon1 blastdb xregexp entrez2cli entrez2 \
	xconnect $(SOBJMGR_LIBS) $(Z_LIB) $(PCRE_LIB) $(LMDB_LIB)

LIBS =  $(BLAST_THIRD_PARTY_LIBS) \
	$(WXWIDGETS_GL_LIBS) $(WXWIDGETS_LIBS) $(OPENGL_LIBS)\
	$(Z_LIBS) $(PNG_LIBS) $(PCRE_LIBS) \
	$(NETWORK_LIBS) \
	$(ORIG_LIBS)

####
#  Linux build: add the following to use MESA
####
#  for REQUIRES:
#	MESA
#
#  for LIB:
#	gui_glmesa-static gui_opengl gui_print gui_objutils-static gui_utils-static ximage \
#
#  for LIBS:
#	$(IMAGE_LIBS) $(OSMESA_STATIC_LIBS) $(BZ2_LIBS) $(DL_LIBS) \
####

# for distribution on linux/gcc, do:
#   gcc  ....  -lpthread -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
