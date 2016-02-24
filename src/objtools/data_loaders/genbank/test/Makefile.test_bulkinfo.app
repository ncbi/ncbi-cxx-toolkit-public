#################################
# $Id$
#################################

APP = test_bulkinfo
SRC = test_bulkinfo bulkinfo_tester
LIB = xobjutil $(OBJMGR_LIBS) ncbi_xdbapi_ftds $(FTDS_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids wgs.ids all_readers.sh ref

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -reference ref/0.gi.txt /CHECK_NAME=test_bulkinfo_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -reference ref/0.acc.txt /CHECK_NAME=test_bulkinfo_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -reference ref/0.label.txt /CHECK_NAME=test_bulkinfo_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -reference ref/0.taxid.txt /CHECK_NAME=test_bulkinfo_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -reference ref/0.length.txt /CHECK_NAME=test_bulkinfo_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -reference ref/0.type.txt /CHECK_NAME=test_bulkinfo_type
CHECK_CMD = all_readers.sh test_bulkinfo -type state -reference ref/0.state.txt /CHECK_NAME=test_bulkinfo_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -reference ref/0.hash.txt /CHECK_NAME=test_bulkinfo_hash

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist wgs.ids -reference ref/wgs.gi.txt /CHECK_NAME=test_bulkinfo_wgs_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist wgs.ids -reference ref/wgs.acc.txt /CHECK_NAME=test_bulkinfo_wgs_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist wgs.ids -reference ref/wgs.label.txt /CHECK_NAME=test_bulkinfo_wgs_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist wgs.ids -reference ref/wgs.taxid.txt /CHECK_NAME=test_bulkinfo_wgs_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist wgs.ids -reference ref/wgs.length.txt /CHECK_NAME=test_bulkinfo_wgs_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist wgs.ids -reference ref/wgs.type.txt /CHECK_NAME=test_bulkinfo_wgs_type
CHECK_CMD = all_readers.sh -xid2 test_bulkinfo -type state -idlist wgs.ids -reference ref/wgs.state.txt /CHECK_NAME=test_bulkinfo_wgs_state
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type state -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_state_id2
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist wgs.ids -reference ref/wgs.hash.txt /CHECK_NAME=test_bulkinfo_wgs_hash

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist bad_len.ids -reference ref/bad_len.gi.txt /CHECK_NAME=test_bulkinfo_bad_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist bad_len.ids -reference ref/bad_len.acc.txt /CHECK_NAME=test_bulkinfo_bad_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist bad_len.ids -reference ref/bad_len.label.txt /CHECK_NAME=test_bulkinfo_bad_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist bad_len.ids -reference ref/bad_len.taxid.txt /CHECK_NAME=test_bulkinfo_bad_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist bad_len.ids -reference ref/bad_len.length.txt /CHECK_NAME=test_bulkinfo_bad_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist bad_len.ids -reference ref/bad_len.type.txt /CHECK_NAME=test_bulkinfo_bad_type
CHECK_CMD = all_readers.sh -xid2 test_bulkinfo -type state -idlist bad_len.ids -reference ref/bad_len.state.txt /CHECK_NAME=test_bulkinfo_bad_state
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type state -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_state_id2
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist bad_len.ids -reference ref/bad_len.hash.txt /CHECK_NAME=test_bulkinfo_bad_hash

CHECK_TIMEOUT = 400

WATCHERS = vasilche
