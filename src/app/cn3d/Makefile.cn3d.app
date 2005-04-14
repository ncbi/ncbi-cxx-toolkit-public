# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

REQUIRES = objects wxWindows ctools OpenGL C-Toolkit

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
	viewer_window_base \
	wx_tools

LIB =	xstruct_dp xblast \
	cdd ncbimime cn3d mmdb scoremat seqset seq seqcode sequtil \
	pub medline biblio general taxon1 \
	xser xutil xctools xconnect xncbi

CPPFLAGS = \
	$(ORIG_CPPFLAGS) \
	$(WXWIN_INCLUDE) \
	$(NCBI_C_INCLUDE)

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS = $(FAST_LDFLAGS)

NCBI_C_LIBS = -lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi

LIBS =	$(WXWIN_LIBS) $(WXWIN_GL_LIBS) \
	$(NCBI_C_LIBPATH) \
	$(NCBI_C_LIBS) \
	$(ORIG_LIBS)


# for distribution on linux/gcc, do:
#   gcc  ....  -lpthread -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
