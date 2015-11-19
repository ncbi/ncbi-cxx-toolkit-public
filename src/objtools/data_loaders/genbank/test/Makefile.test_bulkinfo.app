#################################
# $Id$
#################################

APP = test_bulkinfo
SRC = test_bulkinfo bulkinfo_tester
LIB = xobjutil $(OBJMGR_LIBS) ncbi_xdbapi_ftds $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids wgs.ids all_readers.sh

CHECK_CMD = all_readers.sh test_bulkinfo -type gi /CHECK_NAME=test_bulkinfo_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc /CHECK_NAME=test_bulkinfo_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label /CHECK_NAME=test_bulkinfo_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid /CHECK_NAME=test_bulkinfo_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length /CHECK_NAME=test_bulkinfo_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type /CHECK_NAME=test_bulkinfo_type
CHECK_CMD = all_readers.sh test_bulkinfo -type state /CHECK_NAME=test_bulkinfo_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash /CHECK_NAME=test_bulkinfo_hash

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_type
CHECK_CMD = all_readers.sh test_bulkinfo -type state -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_hash

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_type
CHECK_CMD = all_readers.sh test_bulkinfo -type state -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_bad_hash

CHECK_TIMEOUT = 400

WATCHERS = vasilche
