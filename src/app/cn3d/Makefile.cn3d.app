# $Id$
# Author:  Paul Thiessen

# Build application "cn3d"
#################################

APP = cn3d

OBJ = \
	alignment_manager \
	alignment_set \
	atom_set \
	block_multiple_alignment \
	bond \
	chemical_graph \
	cn3d_colors \
	cn3d_main_wxwin \
	cn3d_threader \
	conservation_colorer \
	coord_set \
	messenger \
	molecule \
	object_3d \
	opengl_renderer \
	periodic_table \
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
	style_manager \
	update_viewer \
	update_viewer_window \
	vector_math \
	viewer_base \
	viewer_window_base \
	wx_tools

LIB = \
	cdd \
	ncbimime \
	mmdb1 \
	mmdb2 \
	mmdb3 \
	pub \
	seq \
	seqalign \
	seqblock \
	seqfeat \
	seqloc \
	seqres \
	seqset \
	general \
	medline \
	biblio \
	xser \
	xncbi \
	xutil \
	xctools \
	xconnect

CPPFLAGS = $(ORIG_CPPFLAGS) \
	-I$(includedir)/gui_ctools \
	$(NCBI_C_INCLUDE) \
	-D__WXGTK__ \
	-I$(HOME)/Programs/wxGTK-2.2.6/install/include

LIBS = $(ORIG_LIBS) \
	$(NCBI_C_LIBPATH) \
	-L$(HOME)/Programs/wxGTK-2.2.6/install/lib \
	-lwx_gtk -lwx_gtk_gl -lGL -lGLU \
	-L$(HOME)/Programs/GTK-1.2/install/lib \
	-L/usr/openwin/lib -R/usr/openwin/lib \
	-lgtk -lgdk -lgmodule -lglib -ldl -lXext \
	-lX11 -lsocket -lnsl -lm -lposix4 \
	-lncbimmdb -lncbiid1 -lnetcli -lncbitool -lncbiobj -lncbi
