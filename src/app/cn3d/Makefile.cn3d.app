# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

REQUIRES = objects wxWindows ctools OpenGL

APP = cn3d

SRC = \
	alignment_manager \
	alignment_set \
	annotate_dialog \
	atom_set \
	block_multiple_alignment \
	bond \
	cdd_annot_dialog \
	cdd_ref_dialog \
	cdd_splash_dialog \
	chemical_graph \
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
	taxonomy_tree \
	update_viewer \
	update_viewer_window \
	vector_math \
	viewer_base \
	viewer_window_base \
	wx_tools

LIB = \
	cdd ncbimime cn3d mmdb1 mmdb2 mmdb3 seqset \
	$(SEQ_LIBS) pub medline	biblio general taxon1 \
	xser xutil xctools xconnect xncbi

CPPFLAGS = \
	$(ORIG_CPPFLAGS) \
	-I$(srcdir)/.. \
	$(WXWIN_INCLUDE) \
	$(NCBI_C_INCLUDE)

LIBS = \
	$(WXWIN_LIBS) $(WXWIN_GL_LIBS) \
	$(NCBI_C_LIBPATH) \
	-lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi \
	$(ORIG_LIBS)


# for distribution on linux/gcc, do:
#   gcc  ....  -lpthread -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic
