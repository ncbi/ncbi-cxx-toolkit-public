#################################
# $Id$
#################################

APP = test_bulkinfo
SRC = test_bulkinfo bulkinfo_tester
LIB = xobjutil ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = supp.ids bad_len.ids wgs.ids wgs_vdb.ids wgs_prot.ids all_readers.sh ref test_bulkinfo_log.ini

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -reference ref/0.gi.txt /CHECK_NAME=test_bulkinfo_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -reference ref/0.acc.txt /CHECK_NAME=test_bulkinfo_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -reference ref/0.label.txt /CHECK_NAME=test_bulkinfo_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -reference ref/0.taxid.txt /CHECK_NAME=test_bulkinfo_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -reference ref/0.length.txt /CHECK_NAME=test_bulkinfo_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -reference ref/0.type.txt /CHECK_NAME=test_bulkinfo_type
CHECK_CMD = all_readers.sh test_bulkinfo -conffile test_bulkinfo_log.ini -type state -reference ref/0.state.txt /CHECK_NAME=test_bulkinfo_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -reference ref/0.hash.txt /CHECK_NAME=test_bulkinfo_hash
CHECK_CMD = all_readers.sh test_bulkinfo -type bioseq -reference ref/0.bioseq.txt /CHECK_NAME=test_bulkinfo_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo -type sequence -reference ref/0.sequence.txt /CHECK_NAME=test_bulkinfo_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type cdd -reference ref/0.cdd.txt /CHECK_NAME=test_bulkinfo_cdd

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist wgs.ids -reference ref/wgs.gi.txt /CHECK_NAME=test_bulkinfo_wgs_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist wgs.ids -reference ref/wgs.acc.txt /CHECK_NAME=test_bulkinfo_wgs_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist wgs.ids -reference ref/wgs.label.txt /CHECK_NAME=test_bulkinfo_wgs_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist wgs.ids -reference ref/wgs.taxid.txt /CHECK_NAME=test_bulkinfo_wgs_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist wgs.ids -reference ref/wgs.length.txt /CHECK_NAME=test_bulkinfo_wgs_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist wgs.ids -reference ref/wgs.type.txt /CHECK_NAME=test_bulkinfo_wgs_type
CHECK_CMD = all_readers.sh -no-vdb-wgs test_bulkinfo -type state -idlist wgs.ids -reference ref/wgs.state.txt /CHECK_NAME=test_bulkinfo_wgs_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type state -idlist wgs.ids /CHECK_NAME=test_bulkinfo_wgs_state_id2
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist wgs.ids -reference ref/wgs.hash.txt /CHECK_NAME=test_bulkinfo_wgs_hash
CHECK_CMD = all_readers.sh test_bulkinfo -type bioseq -idlist wgs.ids -reference ref/wgs.bioseq.txt /CHECK_NAME=test_bulkinfo_wgs_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo -type sequence -idlist wgs.ids -reference ref/wgs.sequence.txt /CHECK_NAME=test_bulkinfo_wgs_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type cdd -idlist wgs.ids -reference ref/wgs.cdd.txt /CHECK_NAME=test_bulkinfo_wgs_cdd

CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type gi -idlist wgs_vdb.ids -reference ref/wgs_vdb.gi.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_gi
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type acc -idlist wgs_vdb.ids -reference ref/wgs_vdb.acc.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_acc
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type label -idlist wgs_vdb.ids -reference ref/wgs_vdb.label.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_label
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type taxid -idlist wgs_vdb.ids -reference ref/wgs_vdb.taxid.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_taxid
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type length -idlist wgs_vdb.ids -reference ref/wgs_vdb.length.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_length
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type type -idlist wgs_vdb.ids -reference ref/wgs_vdb.type.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_type
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type state -idlist wgs_vdb.ids /CHECK_NAME=test_bulkinfo_wgs_vdb_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type hash -idlist wgs_vdb.ids -reference ref/wgs_vdb.hash.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_hash
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type bioseq -idlist wgs_vdb.ids -reference ref/wgs_vdb.bioseq.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_bioseq
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type sequence -idlist wgs_vdb.ids -reference ref/wgs_vdb.sequence.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_sequence
CHECK_CMD = all_readers.sh -vdb-wgs -id2 test_bulkinfo -type cdd -idlist wgs_vdb.ids -reference ref/wgs_vdb.cdd.txt /CHECK_NAME=test_bulkinfo_wgs_vdb_cdd

CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type gi -idlist wgs_prot.ids -reference ref/wgs_prot.gi.txt /CHECK_NAME=test_bulkinfo_wgs_prot_gi
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type acc -idlist wgs_prot.ids -reference ref/wgs_prot.acc.txt /CHECK_NAME=test_bulkinfo_wgs_prot_acc
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type label -idlist wgs_prot.ids -reference ref/wgs_prot.label.txt /CHECK_NAME=test_bulkinfo_wgs_prot_label
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type taxid -idlist wgs_prot.ids -reference ref/wgs_prot.taxid.txt /CHECK_NAME=test_bulkinfo_wgs_prot_taxid
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type length -idlist wgs_prot.ids -reference ref/wgs_prot.length.txt /CHECK_NAME=test_bulkinfo_wgs_prot_length
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type type -idlist wgs_prot.ids -reference ref/wgs_prot.type.txt /CHECK_NAME=test_bulkinfo_wgs_prot_type
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type state -idlist wgs_prot.ids /CHECK_NAME=test_bulkinfo_wgs_prot_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type hash -idlist wgs_prot.ids -reference ref/wgs_prot.hash.txt /CHECK_NAME=test_bulkinfo_wgs_prot_hash
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type bioseq -idlist wgs_prot.ids -reference ref/wgs_prot.bioseq.txt /CHECK_NAME=test_bulkinfo_wgs_prot_bioseq
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo -type sequence -idlist wgs_prot.ids -reference ref/wgs_prot.sequence.txt /CHECK_NAME=test_bulkinfo_wgs_prot_sequence
CHECK_CMD = all_readers.sh -vdb-wgs -id2 test_bulkinfo -type cdd -idlist wgs_prot.ids -reference ref/wgs_prot.cdd.txt /CHECK_NAME=test_bulkinfo_wgs_prot_cdd

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist bad_len.ids -reference ref/bad_len.gi.txt /CHECK_NAME=test_bulkinfo_bad_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist bad_len.ids -reference ref/bad_len.acc.txt /CHECK_NAME=test_bulkinfo_bad_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist bad_len.ids -reference ref/bad_len.label.txt /CHECK_NAME=test_bulkinfo_bad_label
CHECK_CMD = all_readers.sh test_bulkinfo -type taxid -idlist bad_len.ids -reference ref/bad_len.taxid.txt /CHECK_NAME=test_bulkinfo_bad_taxid
CHECK_CMD = all_readers.sh test_bulkinfo -type length -idlist bad_len.ids -reference ref/bad_len.length.txt /CHECK_NAME=test_bulkinfo_bad_length
CHECK_CMD = all_readers.sh test_bulkinfo -type type -idlist bad_len.ids -reference ref/bad_len.type.txt /CHECK_NAME=test_bulkinfo_bad_type
CHECK_CMD = all_readers.sh test_bulkinfo -type state -idlist bad_len.ids -reference ref/bad_len.state.txt /CHECK_NAME=test_bulkinfo_bad_state
CHECK_CMD = all_readers.sh test_bulkinfo -type hash -idlist bad_len.ids -reference ref/bad_len.hash.txt /CHECK_NAME=test_bulkinfo_bad_hash
CHECK_CMD = all_readers.sh test_bulkinfo -type bioseq -idlist bad_len.ids -reference ref/bad_len.bioseq.txt /CHECK_NAME=test_bulkinfo_bad_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo -type sequence -idlist bad_len.ids -reference ref/bad_len.sequence.txt /CHECK_NAME=test_bulkinfo_bad_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type cdd -idlist bad_len.ids -reference ref/bad_len.cdd.txt /CHECK_NAME=test_bulkinfo_bad_cdd

CHECK_CMD = all_readers.sh test_bulkinfo -type gi -idlist supp.ids -reference ref/supp.gi.txt /CHECK_NAME=test_bulkinfo_supp_gi
CHECK_CMD = all_readers.sh test_bulkinfo -type acc -idlist supp.ids -reference ref/supp.acc.txt /CHECK_NAME=test_bulkinfo_supp_acc
CHECK_CMD = all_readers.sh test_bulkinfo -type label -idlist supp.ids -reference ref/supp.label.txt /CHECK_NAME=test_bulkinfo_supp_label
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type taxid -idlist supp.ids -reference ref/supp.taxid.txt /CHECK_NAME=test_bulkinfo_supp_taxid
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type length -idlist supp.ids -reference ref/supp.length.txt /CHECK_NAME=test_bulkinfo_supp_length
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type type -idlist supp.ids -reference ref/supp.type.txt /CHECK_NAME=test_bulkinfo_supp_type
CHECK_CMD = all_readers.sh -psg test_bulkinfo -type state -idlist supp.ids -reference ref/supp.state.txt /CHECK_NAME=test_bulkinfo_supp_state
CHECK_CMD = all_readers.sh -id2 test_bulkinfo -type hash -idlist supp.ids -reference ref/supp.hash.txt /CHECK_NAME=test_bulkinfo_supp_hash

CHECK_TIMEOUT = 400

WATCHERS = vasilche
