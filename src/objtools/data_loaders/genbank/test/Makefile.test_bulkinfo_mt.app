#################################
# $Id$
#################################

APP = test_bulkinfo_mt
SRC = test_bulkinfo_mt bulkinfo_tester
LIB = xobjutil $(OBJMGR_LIBS) test_mt ncbi_xdbapi_ftds $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids wgs.ids all_readers.sh

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi /CHECK_NAME=test_bulkinfo_mt_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc /CHECK_NAME=test_bulkinfo_mt_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label /CHECK_NAME=test_bulkinfo_mt_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid /CHECK_NAME=test_bulkinfo_mt_taxid
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type length -threads 8 /CHECK_NAME=test_bulkinfo_mt_length
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type type -threads 8 /CHECK_NAME=test_bulkinfo_mt_type
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state /CHECK_NAME=test_bulkinfo_mt_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash /CHECK_NAME=test_bulkinfo_mt_hash

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_types
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_hash

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_types
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash -idlist bad_len.ids /CHECK_NAME=test_bulkinfo_mt_bad_hash

CHECK_TIMEOUT = 400

WATCHERS = vasilche
