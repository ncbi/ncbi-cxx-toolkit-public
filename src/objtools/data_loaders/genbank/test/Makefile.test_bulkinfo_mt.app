#################################
# $Id$
#################################

APP = test_bulkinfo_mt
SRC = test_bulkinfo_mt bulkinfo_tester
LIB = xobjutil test_mt ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = bad_len.ids wgs.ids wgs_vdb.ids wgs_prot.ids supp.ids all_readers.sh ref
CHECK_REQUIRES = disabled

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -reference ref/0.gi.txt /CHECK_NAME=test_bulkinfo_mt_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -reference ref/0.acc.txt /CHECK_NAME=test_bulkinfo_mt_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -reference ref/0.label.txt /CHECK_NAME=test_bulkinfo_mt_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -reference ref/0.taxid.txt /CHECK_NAME=test_bulkinfo_mt_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -threads 8 -reference ref/0.length.txt /CHECK_NAME=test_bulkinfo_mt_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -threads 8 -reference ref/0.type.txt /CHECK_NAME=test_bulkinfo_mt_type
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state -reference ref/0.state.txt /CHECK_NAME=test_bulkinfo_mt_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash -no-recalc -reference ref/0.hash.txt /CHECK_NAME=test_bulkinfo_mt_hash
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type bioseq -reference ref/0.bioseq.txt /CHECK_NAME=test_bulkinfo_mt_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type sequence -reference ref/0.sequence.txt /CHECK_NAME=test_bulkinfo_mt_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type cdd -reference ref/0.cdd.txt /CHECK_NAME=test_bulkinfo_mt_cdd

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist wgs.ids -reference ref/wgs.gi.txt /CHECK_NAME=test_bulkinfo_mt_wgs_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist wgs.ids -reference ref/wgs.acc.txt /CHECK_NAME=test_bulkinfo_mt_wgs_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist wgs.ids -reference ref/wgs.label.txt /CHECK_NAME=test_bulkinfo_mt_wgs_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -idlist wgs.ids -reference ref/wgs.taxid.txt /CHECK_NAME=test_bulkinfo_mt_wgs_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -idlist wgs.ids -reference ref/wgs.length.txt /CHECK_NAME=test_bulkinfo_mt_wgs_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -idlist wgs.ids -reference ref/wgs.type.txt /CHECK_NAME=test_bulkinfo_mt_wgs_types
CHECK_CMD = all_readers.sh -no-vdb-wgs test_bulkinfo_mt -type state -idlist wgs.ids -reference ref/wgs.state.txt /CHECK_NAME=test_bulkinfo_mt_wgs_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type state -idlist wgs.ids /CHECK_NAME=test_bulkinfo_mt_wgs_state_vdb
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash -idlist wgs.ids -no-recalc -reference ref/wgs.hash.txt /CHECK_NAME=test_bulkinfo_mt_wgs_hash
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type bioseq -idlist wgs.ids -reference ref/wgs.bioseq.txt /CHECK_NAME=test_bulkinfo_mt_wgs_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type sequence -idlist wgs.ids -reference ref/wgs.sequence.txt /CHECK_NAME=test_bulkinfo_mt_wgs_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type cdd -idlist wgs.ids -reference ref/wgs.cdd.txt /CHECK_NAME=test_bulkinfo_mt_wgs_cdd

CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type gi -idlist wgs_vdb.ids -reference ref/wgs_vdb.gi.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_gi
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type acc -idlist wgs_vdb.ids -reference ref/wgs_vdb.acc.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_acc
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type label -idlist wgs_vdb.ids -reference ref/wgs_vdb.label.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_label
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type taxid -idlist wgs_vdb.ids -reference ref/wgs_vdb.taxid.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_taxid
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type length -idlist wgs_vdb.ids -reference ref/wgs_vdb.length.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_length
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type type -idlist wgs_vdb.ids -reference ref/wgs_vdb.type.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_types
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type state -idlist wgs_vdb.ids /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type hash -idlist wgs_vdb.ids -no-recalc -reference ref/wgs_vdb.hash.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_hash
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type bioseq -idlist wgs_vdb.ids -reference ref/wgs_vdb.bioseq.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_bioseq
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type sequence -idlist wgs_vdb.ids -reference ref/wgs_vdb.sequence.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_sequence
CHECK_CMD = all_readers.sh -vdb-wgs -id2 test_bulkinfo_mt -type cdd -idlist wgs_vdb.ids -reference ref/wgs_vdb.cdd.txt /CHECK_NAME=test_bulkinfo_mt_wgs_vdb_cdd

CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type gi -idlist wgs_prot.ids -reference ref/wgs_prot.gi.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_gi
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type acc -idlist wgs_prot.ids -reference ref/wgs_prot.acc.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_acc
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type label -idlist wgs_prot.ids -reference ref/wgs_prot.label.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_label
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type taxid -idlist wgs_prot.ids -reference ref/wgs_prot.taxid.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_taxid
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type length -idlist wgs_prot.ids -reference ref/wgs_prot.length.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_length
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type type -idlist wgs_prot.ids -reference ref/wgs_prot.type.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_types
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type state -idlist wgs_prot.ids /CHECK_NAME=test_bulkinfo_mt_wgs_prot_state
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type hash -idlist wgs_prot.ids -no-recalc -reference ref/wgs_prot.hash.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_hash
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type bioseq -idlist wgs_prot.ids -reference ref/wgs_prot.bioseq.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_bioseq
CHECK_CMD = all_readers.sh -vdb-wgs test_bulkinfo_mt -type sequence -idlist wgs_prot.ids -reference ref/wgs_prot.sequence.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_sequence
CHECK_CMD = all_readers.sh -vdb-wgs -id2 test_bulkinfo_mt -type cdd -idlist wgs_prot.ids -reference ref/wgs_prot.cdd.txt /CHECK_NAME=test_bulkinfo_mt_wgs_prot_cdd

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist bad_len.ids -reference ref/bad_len.gi.txt /CHECK_NAME=test_bulkinfo_mt_bad_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist bad_len.ids -reference ref/bad_len.acc.txt /CHECK_NAME=test_bulkinfo_mt_bad_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist bad_len.ids -reference ref/bad_len.label.txt /CHECK_NAME=test_bulkinfo_mt_bad_label
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type taxid -idlist bad_len.ids -reference ref/bad_len.taxid.txt /CHECK_NAME=test_bulkinfo_mt_bad_taxid
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type length -idlist bad_len.ids -reference ref/bad_len.length.txt /CHECK_NAME=test_bulkinfo_mt_bad_length
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type type -idlist bad_len.ids -reference ref/bad_len.type.txt /CHECK_NAME=test_bulkinfo_mt_bad_types
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type state -idlist bad_len.ids -reference ref/bad_len.state.txt /CHECK_NAME=test_bulkinfo_mt_bad_state
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type hash -idlist bad_len.ids -reference ref/bad_len.hash.txt /CHECK_NAME=test_bulkinfo_mt_bad_hash
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type bioseq -idlist bad_len.ids -reference ref/bad_len.bioseq.txt /CHECK_NAME=test_bulkinfo_mt_bad_bioseq
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type sequence -idlist bad_len.ids -reference ref/bad_len.sequence.txt /CHECK_NAME=test_bulkinfo_mt_bad_sequence
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type cdd -idlist bad_len.ids -reference ref/bad_len.cdd.txt /CHECK_NAME=test_bulkinfo_mt_bad_cdd

CHECK_CMD = all_readers.sh test_bulkinfo_mt -type gi -idlist supp.ids -reference ref/supp.gi.txt /CHECK_NAME=test_bulkinfo_mt_supp_gi
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type acc -idlist supp.ids -reference ref/supp.acc.txt /CHECK_NAME=test_bulkinfo_mt_supp_acc
CHECK_CMD = all_readers.sh test_bulkinfo_mt -type label -idlist supp.ids -reference ref/supp.label.txt /CHECK_NAME=test_bulkinfo_mt_supp_label
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type taxid -idlist supp.ids -reference ref/supp.taxid.txt /CHECK_NAME=test_bulkinfo_mt_supp_taxid
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type length -idlist supp.ids -reference ref/supp.length.txt /CHECK_NAME=test_bulkinfo_mt_supp_length
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type type -idlist supp.ids -reference ref/supp.type.txt /CHECK_NAME=test_bulkinfo_mt_supp_type
CHECK_CMD = all_readers.sh -psg test_bulkinfo_mt -type state -idlist supp.ids -reference ref/supp.state.txt /CHECK_NAME=test_bulkinfo_mt_supp_state
CHECK_CMD = all_readers.sh -id2 test_bulkinfo_mt -type hash -idlist supp.ids -reference ref/supp.hash.txt /CHECK_NAME=test_bulkinfo_mt_supp_hash

CHECK_TIMEOUT = 400

WATCHERS = vasilche
