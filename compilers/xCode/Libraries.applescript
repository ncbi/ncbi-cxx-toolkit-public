(*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Vlad Lebedev
 *
 * File Description:
 * C++ Toolkit Libraries info Script
 *
 *)


global AllLibraries -- All libbaries to build
global AllTests -- All tests to build
global AllConsoleTools -- All console tools (tests and demos) to build
global AllApplications -- All GUI applications to build


(* Libraries for linking (note the extra space before the lib name! )*)
property Z_LIBS : " bz2 z"
property IMG_LIBS : " jpeg png tiff gif"
property FLTK_LIBS : " fltk_images fltk_gl fltk"
property BDB_LIBS : " db"
property SQLITE_LIBS : " sqlite"
property gui2link : BDB_LIBS & SQLITE_LIBS & FLTK_LIBS & Z_LIBS & IMG_LIBS

(*
Library definition rules:
name: Name of the library to build
path: source path relative to the source root (users:lebedev:c++:src:)
inc: include only listed files
exc: include all but listed files
bundle: compile as a loadable mach-o bundle (like GBench plugin)
dep: a list of dependancies
asn1: true if source is generated from ASN1 files. Will add a shell script build phase and a datatool dependency
*)



(* Libraries definitions *)
-- Core
property xncbi : {name:"xncbi", path:"corelib", exc:{"test_mt.cpp", "ncbi_os_mac.cpp"}}
property xcgi : {name:"xcgi", path:"cgi", exc:{"fcgi_run.cpp", "fcgibuf.cpp"}}
property dbapi : {name:"dbapi", path:"dbapi"}
property dbapi_cache : {name:"dbapi_cache", path:"dbapi:cache"}
property dbapi_driver : {name:"dbapi_driver", path:"dbapi:driver"}
property xhtml : {name:"xhtml", path:"html"}
property xconnect : {name:"xconnect", path:"connect", exc:{"ncbi_lbsm_ipc.c", "ncbi_lbsm.c", "ncbi_lbsmd.c", "threaded_server.cpp"}}
--property cserial : {name:"cserial", path:"serial", inc:{"asntypes.cpp", "serialasn.cpp"}}
property xser : {name:"xser", path:"serial", exc:{"asntypes.cpp", "object.cpp", "objstrb.cpp", "rtti.cpp", "serialasn.cpp"}}
property xutil : {name:"xutil", path:"util"}
property bdb : {name:"bdb", path:"bdb", exc:{"bdb_query_bison.tab.c", "bdb_query_lexer.cpp"}}
property xsqlite : {name:"xsqlite", path:"sqlite"}
property xregexp : {name:"xregexp", path:"util:regexp"}
property ximage : {name:"ximage", path:"util:image"}
property xcompress : {name:"xcompress", path:"util:compress"}
property tables : {name:"tables", path:"util:tables", inc:{"raw_scoremat.c"}}
property sequtil : {name:"sequtil", path:"util:sequtil"}
property creaders : {name:"creaders", path:"util:creaders"}
-- Algo
property xalgoalign : {name:"xalgoalign", path:"algo:align"}
property xalgosplign : {name:"xalgosplign", path:"algo:align:splign"}
property xalgoseq : {name:"xalgoseq", path:"algo:sequence"}
property blast : {name:"blast", path:"algo:blast:core"}
property xblast : {name:"xblast", path:"algo:blast:api"}
property xalgognomon : {name:"xalgognomon", path:"algo:gnomon"}
property xalgophytree : {name:"xalgophytree", path:"algo:phy_tree"}
property fastme : {name:"fastme", path:"algo:phy_tree:fastme"}

-- Objects
property access : {name:"access", path:"objects:access", inc:{"access__.cpp", "access___.cpp"}, asn1:true}
property biblio : {name:"biblio", path:"objects:biblio", inc:{"biblio__.cpp", "biblio___.cpp", "label_util.cpp"}, asn1:true}
property biotree : {name:"biotree", path:"objects:biotree", inc:{"biotree__.cpp", "biotree___.cpp"}, asn1:true}
property xnetblast : {name:"xnetblast", path:"objects:blast", inc:{"blast__.cpp", "blast___.cpp"}, asn1:true}
property xnetblastcli : {name:"xnetblastcli", path:"objects:blast", inc:{"blastclient.cpp", "blastclient_.cpp"}, asn1:true}
property blastdb : {name:"blastdb", path:"objects:blastdb", inc:{"blastdb__.cpp", "blastdb___.cpp"}, asn1:true}
property cdd : {name:"cdd", path:"objects:cdd", inc:{"cdd__.cpp", "cdd___.cpp"}, asn1:true}
property cn3d : {name:"cn3d", path:"objects:cn3d", inc:{"cn3d__.cpp", "cn3d___.cpp"}, asn1:true}
property docsum : {name:"docsum", path:"objects:docsum", inc:{"docsum__.cpp", "docsum___.cpp"}, asn1:true}
property entrez2 : {name:"entrez2", path:"objects:entrez2", inc:{"entrez2__.cpp", "entrez2___.cpp"}, asn1:true}
property entrez2cli : {name:"entrez2cli", path:"objects:entrez2", inc:{"entrez2_client.cpp", "entrez2_client_.cpp"}, asn1:true}
property entrezgene : {name:"entrezgene", path:"objects:entrezgene", inc:{"entrezgene__.cpp", "entrezgene___.cpp"}, asn1:true}
property featdef : {name:"featdef", path:"objects:featdef", inc:{"featdef__.cpp", "featdef___.cpp"}, asn1:true}
property gbseq : {name:"gbseq", path:"objects:gbseq", inc:{"gbseq__.cpp", "gbseq___.cpp"}, asn1:true}
property general : {name:"general", path:"objects:general", inc:{"general__.cpp", "general___.cpp"}, asn1:true}
property id1 : {name:"id1", path:"objects:id1", inc:{"id1__.cpp", "id1___.cpp"}, asn1:true}
property id1cli : {name:"id1cli", path:"objects:id1", inc:{"id1_client.cpp", "id1_client_.cpp"}, asn1:true}
property id2 : {name:"id2", path:"objects:id2", inc:{"id2__.cpp", "id2___.cpp"}, asn1:true}
property id2cli : {name:"id2cli", path:"objects:id2", inc:{"id2_client.cpp", "id2_client_.cpp"}, asn1:true}
property medlars : {name:"medlars", path:"objects:medlars", inc:{"medlars__.cpp", "medlars___.cpp"}, asn1:true}
property medline : {name:"medline", path:"objects:medline", inc:{"medline__.cpp", "medline___.cpp"}, asn1:true}
property mim : {name:"mim", path:"objects:mim", inc:{"mim__.cpp", "mim___.cpp"}, asn1:true}
property mla : {name:"mla", path:"objects:mla", inc:{"mla__.cpp", "mla___.cpp"}, asn1:true}
property mlacli : {name:"mlacli", path:"objects:mla", inc:{"mla_client.cpp", "mla_client_.cpp"}, asn1:true}
property mmdb1 : {name:"mmdb1", path:"objects:mmdb1", inc:{"mmdb1__.cpp", "mmdb1___.cpp"}, asn1:true}
property mmdb2 : {name:"mmdb2", path:"objects:mmdb2", inc:{"mmdb2__.cpp", "mmdb2___.cpp"}, asn1:true}
property mmdb3 : {name:"mmdb3", path:"objects:mmdb3", inc:{"mmdb3__.cpp", "mmdb3___.cpp"}, asn1:true}
property ncbimime : {name:"ncbimime", path:"objects:ncbimime", inc:{"ncbimime__.cpp", "ncbimime___.cpp"}, asn1:true}
property objprt : {name:"objprt", path:"objects:objprt", inc:{"objprt__.cpp", "objprt___.cpp"}, asn1:true}
property omssa : {name:"omssa", path:"objects:omssa", inc:{"omssa__.cpp", "omssa___.cpp"}, asn1:true}
property pcassay : {name:"pcassay", path:"objects:pcassay", inc:{"pcassay__.cpp", "pcassay___.cpp"}, asn1:true}
property pcsubstance : {name:"pcsubstance", path:"objects:pcsubstance", inc:{"pcsubstance__.cpp", "pcsubstance___.cpp"}, asn1:true}
property proj : {name:"proj", path:"objects:proj", inc:{"proj__.cpp", "proj___.cpp"}, asn1:true}
property pub : {name:"pub", path:"objects:pub", inc:{"pub__.cpp", "pub___.cpp"}, asn1:true}
property pubmed : {name:"pubmed", path:"objects:pubmed", inc:{"pubmed__.cpp", "pubmed___.cpp"}, asn1:true}
property scoremat : {name:"scoremat", path:"objects:scoremat", inc:{"scoremat__.cpp", "scoremat___.cpp"}, asn1:true}
property seq : {name:"seq", path:"objects:seq", inc:{"seq__.cpp", "seq___.cpp", "seqport_util.cpp", "seq_id_tree.cpp", "seq_id_handle.cpp", "seq_id_mapper.cpp"}, asn1:true}
property seqalign : {name:"seqalign", path:"objects:seqalign", inc:{"seqalign__.cpp", "seqalign___.cpp"}, asn1:true}
property seqblock : {name:"seqblock", path:"objects:seqblock", inc:{"seqblock__.cpp", "seqblock___.cpp"}, asn1:true}
property seqfeat : {name:"seqfeat", path:"objects:seqfeat", inc:{"seqfeat__.cpp", "seqfeat___.cpp"}, asn1:true}
property seqloc : {name:"seqloc", path:"objects:seqloc", inc:{"seqloc__.cpp", "seqloc___.cpp"}, asn1:true}
property seqres : {name:"seqres", path:"objects:seqres", inc:{"seqres__.cpp", "seqres___.cpp"}, asn1:true}
property seqset : {name:"seqset", path:"objects:seqset", inc:{"seqset__.cpp", "seqset___.cpp"}, asn1:true}
property seqcode : {name:"seqcode", path:"objects:seqcode", inc:{"seqcode__.cpp", "seqcode___.cpp"}, asn1:true}
property seqsplit : {name:"seqsplit", path:"objects:seqsplit", inc:{"seqsplit__.cpp", "seqsplit___.cpp"}, asn1:true}
property submit : {name:"submit", path:"objects:submit", inc:{"submit__.cpp", "submit___.cpp"}, asn1:true}
property taxon1 : {name:"taxon1", path:"objects:taxon1", inc:{"taxon1__.cpp", "taxon1___.cpp", "taxon1.cpp", "cache.cpp", "utils.cpp", "ctreecont.cpp"}, asn1:true}
property tinyseq : {name:"tinyseq", path:"objects:tinyseq", inc:{"tinyseq__.cpp", "tinyseq___.cpp"}, asn1:true}


(* ObjTools libs*)
property xobjmgr : {name:"xobjmgr", path:"objmgr"}
property xobjutil : {name:"xobjutil", path:"objmgr:util"}
property id2_split : {name:"id2_split", path:"objmgr:split", exc:{"split_cache.cpp"}}
property xalnmgr : {name:"xalnmgr", path:"objtools:alnmgr"}
property xcddalignview : {name:"xcddalignview", path:"objtools:cddalignview"}
property xflat : {name:"xflat", path:"objtools:flat"}
property lds : {name:"lds", path:"objtools:lds"}
property lds_admin : {name:"lds_admin", path:"objtools:lds:admin"}
property xvalidate : {name:"xvalidate", path:"objtools:validator"}
property xobjmanip : {name:"xobjmanip", path:"objtools:manip"}
property xobjread : {name:"xobjread", path:"objtools:readers"}
property xobjedit : {name:"xobjedit", path:"objtools:edit"}
property seqdb : {name:"seqdb", path:"objtools:readers:seqdb"}
property xformat : {name:"xformat", path:"objtools:format"}
(* loaders and readers *)
property xloader_lds : {name:"xloader_lds", path:"objtools:data_loaders:lds"}
property xloader_table : {name:"xloader_table", path:"objtools:data_loaders:table"}
property xloader_trace : {name:"xloader_trace", path:"objtools:data_loaders:trace"}
property xloader_genbank : {name:"xloader_genbank", path:"objtools:data_loaders:genbank", inc:{"gbloader.cpp", "gbload_util.cpp", "blob_id.cpp"}}
--property ncbi_xloader_blastdb : {name:"ncbi_xloader_blastdb", path:"objtools:data_loaders:blastdb"} -- readdb.h is missing?
property xloader_cdd : {name:"xloader_cdd", path:"objtools:data_loaders:cdd"}

property xreader : {name:"xreader", path:"objtools:data_loaders:genbank", inc:{"reader.cpp", "reader_snp.cpp", "seqref.cpp", "split_parser.cpp", "request_result.cpp"}}
property xreader_id1 : {name:"xreader_id1", path:"objtools:data_loaders:genbank:readers:id1", inc:{"reader_id1.cpp"}}
property xreader_id1c : {name:"xreader_id1c", path:"objtools:data_loaders:genbank:readers:id1", inc:{"reader_id1_cache.cpp"}}
property xreader_id2 : {name:"xreader_id1", path:"objtools:data_loaders:genbank:readers:id2", inc:{"reader_id2.cpp"}}
property xreader_pubseqos : {name:"xreader_pubseqos", path:"objtools:data_loaders:genbank:readers:pubseqos"}
property xobjwrite : {name:"xobjwrite", path:"objtools:writers"}


(* GUI Libraries *)
property gui__core : {name:"gui__core", path:"gui:core", exc:{"GBProjectHandle.cpp", "GBProject.cpp", "GBWorkspace.cpp", "ProjectHistoryItem.cpp", "ProjectItem.cpp", "ViewDesc.cpp", "FolderInfo.cpp", "ProjectFolder.cpp", "WorkspaceFolder.cpp", "ProjectDescr.cpp", "AbstractProjectItem.cpp"}}
property gui__utils : {name:"gui__utils", path:"gui:utils"}
property gui_objutils : {name:"gui_objutils", path:"gui:objutils"}
property gui_project : {name:"gui_project", path:"gui:core", inc:{"gui_project__.cpp", "gui_project___.cpp"}, asn1:true}
property gui__config : {name:"gui__config", path:"gui:config", inc:{"feat_config.cpp", "settings.cpp", "settings_set.cpp", "feat_color.cpp", "feat_config_list.cpp", "feat_show.cpp", "theme_set.cpp", "layout_chooser.cpp", "feat_decorate.cpp", "config___.cpp", "config__.cpp"}, asn1:true}
property gui_opengl : {name:"gui_opengl", path:"gui:opengl"}
property gui__graph : {name:"gui__graph", path:"gui:graph"}
property gui_print : {name:"gui_print", path:"gui:print"}
property gui_math : {name:"gui_math", path:"gui:math"}
property xgbplugin : {name:"xgbplugin", path:"gui:plugin", inc:{"plugin__.cpp", "plugin___.cpp"}, asn1:true}

property gui_dlg_entry_form : {name:"gui_dlg_entry_form", path:"gui:dialogs:entry_form"}
property gui_dlg_multi_col : {name:"gui_dlg_multi_col", path:"gui:dialogs:col"}
property gui_dlg_registry : {name:"gui_dlg_registry", path:"gui:dialogs:registry"}
property gui_dlg_progress : {name:"gui_dlg_progress", path:"gui:dialogs:progress"}
property gui_dlg_config : {name:"gui_dlg_config", path:"gui:dialogs:config"}
property gui_dlg_featedit : {name:"gui_dlg_featedit", path:"gui:dialogs:edit:feature"}
property gui_dlg_edit : {name:"gui_dlg_edit", path:"gui:dialogs:edit"}

(* Widgets *)
property w_fltable : {name:"w_fltable", path:"gui:widgets:Fl_Table"}
property w_flu : {name:"w_flu", path:"gui:widgets:FLU"}
property w_fltk : {name:"w_fltk", path:"gui:widgets:fl"}
property w_gl : {name:"w_gl", path:"gui:widgets:gl"}
property w_seq : {name:"w_seq", path:"gui:widgets:seq"}
property w_toplevel : {name:"w_toplevel", path:"gui:widgets:toplevel"}
property w_aln_data : {name:"w_aln_data", path:"gui:widgets:aln_data"}
property w_seq_graphic : {name:"w_seq_graphic", path:"gui:widgets:seq_graphic"}
property w_hit_matrix : {name:"w_hit_matrix", path:"gui:widgets:hit_matrix"}
property w_aln_crossaln : {name:"w_aln_crossaln", path:"gui:widgets:aln_crossaln"}
property w_aln_textaln : {name:"w_aln_textaln", path:"gui:widgets:aln_textaln"}
property w_aln_multi : {name:"w_aln_multi", path:"gui:widgets:aln_multiple"}
property w_aln_table : {name:"w_aln_table", path:"gui:widgets:aln_table"}
property w_taxtree : {name:"w_taxtree", path:"gui:widgets:tax_tree"}
property w_taxplot3d : {name:"w_taxplot3d", path:"gui:widgets:taxplot3d"}
property w_workspace : {name:"w_workspace", path:"gui:widgets:workspace"}
property w_phylo_tree : {name:"w_phylo_tree", path:"gui:widgets:phylo_tree"}
property w_serial_browse : {name:"w_serial_browse", path:"gui:widgets:serial_browse"}

(* GUI Plugins *)
property gui_doc_basic : {name:"gui_doc_basic", path:"gui:plugins:doc:basic"}
property gui_doc_table : {name:"gui_doc_table", path:"gui:plugins:doc:table"}
-- algo
property gui_algo_align : {name:"gui_algo_align", path:"gui:plugins:algo:align"}
property gui_algo_validator : {name:"gui_algo_validator", path:"gui:plugins:algo:validator"}
property gui_algo_basic : {name:"gui_algo_basic", path:"gui:plugins:algo:basic"}
property gui_algo_cn3d : {name:"gui_algo_cn3d", path:"gui:plugins:algo:cn3d"}
property gui_algo_external : {name:"gui_algo_external", path:"gui:plugins:algo:external"}
property gui_algo_external_out : {name:"gui_algo_external_out", path:"gui:plugins:algo:basic", inc:{"output_dlg.cpp"}} -- a shared dialog from other plugin
property gui_algo_gnomon : {name:"gui_algo_gnomon", path:"gui:plugins:algo:gnomon"}
property gui_algo_linkout : {name:"gui_algo_linkout", path:"gui:plugins:algo:linkout"}
property gui_algo_phylo : {name:"gui_algo_phylo", path:"gui:plugins:algo:phylo"}
property gui_ncbi_init : {name:"gui_ncbi_init", path:"gui:plugins:algo:init"}
-- view
property gui_view_align : {name:"gui_view_align", path:"gui:plugins:view:align"}
property gui_view_graphic : {name:"gui_view_graphic", path:"gui:plugins:view:graphic"}
property gui_view_phylo_tree : {name:"gui_view_phylo_tree", path:"gui:plugins:view:phylo_tree"}
property gui_view_table : {name:"gui_view_table", path:"gui:plugins:view:table"}
property gui_view_taxplot : {name:"gui_view_taxplot", path:"gui:plugins:view:taxplot"}
property gui_view_text : {name:"gui_view_text", path:"gui:plugins:view:text"}
property gui_view_validator : {name:"gui_view_validator", path:"gui:plugins:view:validator"}


-- Console Tools
property test1 : {name:"test_ncbi_tree", path:"corelib:test", inc:{"test_ncbi_tree.cpp"}, dep:"ncbi_core" & Z_LIBS}
property datatool : {name:"datatool", path:"serial:datatool", dep:"ncbi_core pcre" & Z_LIBS}
-- Tests

(* GUI Applications *)

(* Demo GUI Applications *)
property demo_seqgraphic : {name:"demo_seqgraphic", path:"gui:widgets:seq_graphic:demo", dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_xloader_genbank gui_core gui_utils gui_dialogs gui_widgets gui_widgets_seq gui_config" & gui2link}
property demo_crossaln : {name:"demo_crossaln", path:"gui:widgets:aln_crossaln:demo", dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_xloader_genbank gui_core gui_utils gui_dialogs gui_widgets gui_widgets_seq gui_widgets_aln gui_config" & gui2link}

property demo_hitmatrix : {name:"demo_hitmatrix", path:"gui:widgets:hit_matrix:demo", dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_xloader_genbank gui_core gui_utils gui_dialogs gui_widgets gui_widgets_aln gui_config" & gui2link}

property gbench : {name:"gbench", path:"gui:gbench", exc:{"windows_registry.cpp"}, dep:"ncbi_core ncbi_general ncbi_seq ncbi_seqext gui_core gui_utils gui_dialogs gui_widgets gui_config" & gui2link, gbench:true}
property gbench_plugin_scan : {name:"gbench_plugin_scan", path:"gui:gbench:gbench_plugin_scan", dep:"ncbi_core ncbi_seq ncbi_seqext gui_core gui_utils" & FLTK_LIBS & Z_LIBS & IMG_LIBS}


(*****************************************************************************************)
-- Organize everything into convinient packs --
(*****************************************************************************************)
property ncbi_core : {name:"ncbi_core", libs:{xncbi, xcompress, tables, sequtil, creaders, xregexp, xutil, xconnect, xser}}
property ncbi_bdb : {name:"ncbi_bdb", libs:{bdb}, dep:"ncbi_core"}
property ncbi_dbapi_driver : {name:"ncbi_dbapi_driver", libs:{dbapi_driver}, dep:"ncbi_core"}
property ncbi_dbapi : {name:"ncbi_dbapi", libs:{dbapi, dbapi_cache}, dep:"ncbi_core ncbi_dbapi_driver"}
property ncbi_general : {name:"ncbi_general", libs:{general}, dep:"ncbi_core"}
property ncbi_image : {name:"ncbi_image", libs:{ximage}, dep:"ncbi_core"}

property ncbi_algo : {name:"ncbi_algo", libs:{xalgoalign, xalgosplign, xalgoseq, blast, xblast, xalgognomon, xalgophytree, fastme}, dep:"ncbi_core ncbi_seq ncbi_misc"}
property ncbi_misc : {name:"ncbi_misc", libs:{access, biotree, docsum, entrez2, entrez2cli, entrezgene, featdef, gbseq, mim, objprt, tinyseq, proj, omssa, pcassay, pcsubstance}}
property ncbi_pub : {name:"ncbi_pub", libs:{biblio, medline, medlars, mla, mlacli, pub, pubmed}, dep:"ncbi_core ncbi_general"}
property ncbi_seq : {name:"ncbi_seq", libs:{seq, seqset, seqcode, submit, scoremat, xnetblast, xnetblastcli, blastdb, taxon1, seqsplit, seqres, seqloc, seqfeat, seqblock, seqalign}, dep:"ncbi_core ncbi_general ncbi_pub"}
property ncbi_mmdb : {name:"ncbi_mmdb", libs:{cdd, cn3d, ncbimime, mmdb1, mmdb2, mmdb3}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_seq"}
property ncbi_seqext : {name:"ncbi_seqext", libs:{xflat, xalnmgr, xobjmgr, xobjread, xobjwrite, xobjutil, xobjmanip, xformat, seqdb, id1, id1cli, id2, id2cli, id2_split, xobjedit}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_misc ncbi_seq ncbi_dbapi_driver"}
property ncbi_sqlite : {name:"ncbi_sqlite", libs:{xsqlite}, dep:"ncbi_core"}
property ncbi_validator : {name:"ncbi_validator", libs:{xvalidate}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_seq ncbi_seqext"}
property ncbi_web : {name:"ncbi_web", libs:{xhtml, xcgi}, dep:"ncbi_core"}
property ncbi_lds : {name:"ncbi_lds", libs:{lds, lds_admin}, dep:"ncbi_core ncbi_bdb ncbi_general ncbi_seq ncbi_seqext"}
property ncbi_xreader : {name:"ncbi_xreader", libs:{xreader}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_seq ncbi_seqext"}
property ncbi_xreader_id1 : {name:"ncbi_xreader_id1", libs:{xreader_id1, xreader_id1c}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_seq ncbi_seqext ncbi_xreader"}
property ncbi_xreader_id2 : {name:"ncbi_xreader_id2", libs:{xreader_id2}, dep:"ncbi_core ncbi_general ncbi_pub ncbi_seq ncbi_seqext ncbi_xreader"}
property ncbi_xreader_pubseqos : {name:"ncbi_xreader_pubseqos", libs:{xreader_pubseqos}, dep:"ncbi_core ncbi_dbapi ncbi_dbapi_driver ncbi_pub ncbi_seq ncbi_seqext ncbi_xreader"}
property ncbi_xloader_cdd : {name:"ncbi_xloader_cdd", libs:{xloader_cdd}, dep:"ncbi_core ncbi_pub ncbi_seq ncbi_seqext ncbi_xreader ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_pubseqos ncbi_general"}
property ncbi_xloader_genbank : {name:"ncbi_xloader_genbank", libs:{xloader_genbank}, dep:"ncbi_core ncbi_pub ncbi_seq ncbi_seqext ncbi_xreader ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_pubseqos"}
property ncbi_xloader_lds : {name:"ncbi_xloader_lds", libs:{xloader_lds}, dep:"ncbi_bdb ncbi_core ncbi_general ncbi_lds ncbi_pub ncbi_seq ncbi_seqext"}
property ncbi_xloader_table : {name:"ncbi_xloader_table", libs:{xloader_table}, dep:"ncbi_core ncbi_general ncbi_seq ncbi_seqext ncbi_sqlite"}
property ncbi_xloader_trace : {name:"ncbi_xloader_trace", libs:{xloader_trace}, dep:"ncbi_core ncbi_general ncbi_seq ncbi_seqext"}

-- GUI
property gui_utils : {name:"gui_utils", libs:{gui__utils, gui_objutils, gui_opengl, gui_print, gui_math}, dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_image ncbi_general"}
property gui_config : {name:"gui_config", libs:{gui__config}, dep:"gui_utils ncbi_core ncbi_seq ncbi_seqext"}
property gui_graph : {name:"gui_graph", libs:{gui__graph}, dep:"gui_utils ncbi_core"}
property gui_widgets : {name:"gui_widgets", libs:{w_toplevel, w_workspace, w_fltk, w_gl, w_flu, w_fltable}, dep:"gui_utils ncbi_image ncbi_core"}
property gui_dialogs : {name:"gui_dialogs", libs:{gui_dlg_entry_form, gui_dlg_multi_col, gui_dlg_progress, gui_dlg_config, gui_dlg_featedit, gui_dlg_edit}, dep:"gui_config gui_utils gui_widgets ncbi_core ncbi_seq ncbi_seqext"} -- gui_dlg_registry
property gui_core : {name:"gui_core", libs:{gui__core, xgbplugin, gui_project}, dep:"gui_config gui_dialogs gui_utils gui_widgets ncbi_core ncbi_general ncbi_seq ncbi_seqext"}
property gui_widgets_misc : {name:"gui_widgets_misc", libs:{w_phylo_tree, w_taxplot3d}, dep:"ncbi_algo ncbi_core ncbi_seq ncbi_seqext ncbi_general gui_utils gui_graph gui_widgets"}
property gui_widgets_seq : {name:"gui_widgets_seq", libs:{w_seq_graphic, w_taxtree, w_seq, w_serial_browse}, dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_general gui_config gui_utils gui_widgets"}
property gui_widgets_aln : {name:"gui_widgets_aln", libs:{w_aln_crossaln, w_aln_multi, w_aln_textaln, w_aln_data, w_hit_matrix, w_aln_table}, dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_general gui_config gui_utils gui_graph gui_widgets gui_widgets_seq"} --gui_core
-- PLUG-INS
property algo_align : {name:"algo_align", libs:{gui_algo_align}, dep:"gui_core gui_dialogs gui_utils gui_widgets gui_widgets_seq ncbi_algo ncbi_general ncbi_misc ncbi_seq ncbi_seqext ncbi_bdb ncbi_core" & gui2link, bundle:true}
property algo_basic : {name:"algo_basic", libs:{gui_algo_basic}, dep:"gui_core gui_dialogs gui_utils gui_widgets_seq ncbi_algo ncbi_core ncbi_general ncbi_seq ncbi_seqext ncbi_xloader_cdd" & gui2link, bundle:true}
property algo_cn3d : {name:"algo_cn3d", libs:{gui_algo_cn3d}, dep:"gui_core gui_utils ncbi_core ncbi_general ncbi_seq ncbi_seqext ncbi_mmdb" & gui2link, bundle:true}
property algo_external : {name:"algo_external", libs:{gui_algo_external, gui_algo_external_out}, dep:"gui_core gui_utils ncbi_core ncbi_general ncbi_seq ncbi_seqext ncbi_web" & gui2link, bundle:true}
property algo_gnomon : {name:"algo_gnomon", libs:{gui_algo_gnomon}, dep:"gui_core gui_dialogs gui_utils gui_widgets_seq ncbi_algo ncbi_core ncbi_general ncbi_seq ncbi_seqext" & gui2link, bundle:true}
property algo_init : {name:"algo_init", libs:{gui_ncbi_init}, dep:"gui_core gui_utils gui_dialogs gui_widgets_seq ncbi_bdb ncbi_core ncbi_lds ncbi_seq ncbi_seqext ncbi_xloader_genbank ncbi_xloader_lds ncbi_xreader ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_pubseqos" & gui2link, bundle:true}
property algo_linkout : {name:"algo_linkout", libs:{gui_algo_linkout}, dep:"gui_core gui_utils ncbi_core ncbi_general ncbi_seq ncbi_seqext" & gui2link, bundle:true}
property algo_phylo : {name:"algo_phylo", libs:{gui_algo_phylo}, dep:"gui_core gui_utils ncbi_algo ncbi_core ncbi_seq ncbi_misc ncbi_seqext" & gui2link, bundle:true}
property algo_validator : {name:"algo_validator", libs:{gui_algo_validator}, dep:"gui_core ncbi_core ncbi_seq ncbi_seqext ncbi_validator" & gui2link, bundle:true}
property dload_basic : {name:"dload_basic", libs:{gui_doc_basic}, dep:"gui_core gui_dialogs gui_utils gui_widgets gui_widgets_seq ncbi_algo ncbi_bdb ncbi_core ncbi_lds ncbi_misc ncbi_seq ncbi_seqext ncbi_xloader_genbank" & gui2link, bundle:true}
property dload_table : {name:"dload_table", libs:{gui_doc_table}, dep:"gui_core gui_dialogs gui_utils ncbi_core ncbi_general ncbi_seq ncbi_seqext ncbi_sqlite ncbi_xloader_table" & gui2link, bundle:true}
property view_align : {name:"view_align", libs:{gui_view_align}, dep:"ncbi_core ncbi_seq ncbi_seqext gui_core gui_utils gui_widgets gui_widgets_aln gui_config" & gui2link, bundle:true}
property view_graphic : {name:"view_graphic", libs:{gui_view_graphic}, dep:"ncbi_core ncbi_seq ncbi_seqext gui_core gui_utils gui_dialogs gui_widgets gui_widgets_seq gui_config" & gui2link, bundle:true}
property view_phylotree : {name:"view_phylotree", libs:{gui_view_phylo_tree}, dep:"ncbi_core ncbi_algo ncbi_misc gui_core gui_utils gui_widgets gui_widgets_misc" & gui2link, bundle:true}
property view_table : {name:"view_table", libs:{gui_view_table}, dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_general gui_core gui_utils gui_widgets gui_widgets_aln" & gui2link, bundle:true}
property view_taxplot : {name:"view_taxplot", libs:{gui_view_taxplot}, dep:"ncbi_core ncbi_seq ncbi_seqext ncbi_general gui_core gui_utils gui_widgets gui_widgets_misc gui_dialogs" & gui2link, bundle:true}
property view_text : {name:"view_text", libs:{gui_view_text}, dep:"ncbi_core ncbi_pub ncbi_seq ncbi_seqext ncbi_general gui_core gui_utils gui_widgets gui_widgets_seq gui_dialogs" & gui2link, bundle:true}
property view_validator : {name:"view_validator", libs:{gui_view_validator}, dep:"ncbi_core ncbi_seq ncbi_validator gui_core gui_utils gui_widgets" & gui2link, bundle:true}


-- All Libraries to build
property allLibs : {ncbi_core, ncbi_bdb, ncbi_dbapi_driver, ncbi_dbapi, ncbi_general, ncbi_image, ncbi_misc, ncbi_pub, ncbi_seq, ncbi_mmdb, ncbi_seqext, ncbi_algo, ncbi_sqlite, ncbi_validator, ncbi_web, ncbi_lds, ncbi_xreader, ncbi_xreader_id1, ncbi_xreader_id2, ncbi_xreader_pubseqos, ncbi_xloader_cdd, ncbi_xloader_genbank, ncbi_xloader_lds, ncbi_xloader_table, ncbi_xloader_trace, gui_utils, gui_config, gui_graph, gui_widgets, gui_dialogs, gui_core, gui_widgets_misc, gui_widgets_seq, gui_widgets_aln, algo_align, algo_basic, algo_cn3d, algo_external, algo_gnomon, algo_init, algo_linkout, algo_phylo, algo_validator, dload_basic, dload_table, view_align, view_graphic, view_phylotree, view_table, view_taxplot, view_text, view_validator}

--property allLibs : {ncbi_dbapi_driver}
-- Tools packs
property tests : {test1, gbench_plugin_scan}
property datatools : {datatool}
property allCTools : {tests, datatools}

-- Application packs
property gui_demos : {demo_seqgraphic, demo_crossaln, demo_hitmatrix}
property gui_gbench : {gbench}
property allApps : {gui_demos, gui_gbench}



script ToolkitSource
	on Initialize()
		set AllLibraries to allLibs
		set AllConsoleTools to allCTools
		set AllApplications to allApps
	end Initialize
end script


(*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2004/08/12 15:04:03  lebedev
 * algo_align links to gui_widgets_seq (for SerialBrowser)
 *
 * Revision 1.18  2004/08/11 15:48:46  lebedev
 * dlg_edit and dlg_edit_feature added
 *
 * Revision 1.17  2004/08/10 15:49:09  lebedev
 * gui_dialogs fixed (gui_dlg removed)
 *
 * Revision 1.16  2004/08/10 15:22:07  lebedev
 * Simplify target dependencies for xCode 1.5
 *
 * Revision 1.15  2004/08/04 18:49:28  lebedev
 * ID2 libs added
 *
 * Revision 1.14  2004/08/02 17:42:30  lebedev
 * Added ncbi_misc to algo_phylo for biotree stuff
 *
 * Revision 1.13  2004/07/30 13:50:57  lebedev
 * gui_config added to the list of dependencies for view_align plugin
 *
 * Revision 1.12  2004/07/29 13:16:04  lebedev
 * demo for cross alignment added
 *
 * Revision 1.11  2004/07/22 16:04:33  lebedev
 * added dependency on ncbi_seqext for validator
 *
 * Revision 1.10  2004/07/19 16:02:43  lebedev
 * dbapi_cache added
 *
 * Revision 1.9  2004/07/19 12:57:45  lebedev
 * +ncbi_misc for view_phylotree
 *
 * Revision 1.8  2004/07/16 13:25:23  lebedev
 * objects/seq: seq_id_tree seq_id_handle seq_id_mapper
 *
 * Revision 1.7  2004/07/08 18:43:02  lebedev
 * added xobjwrite to [ncbi_seqext]
 *
 * Revision 1.6  2004/07/07 18:34:26  lebedev
 * Datatool script build phase clean-up
 *
 * Revision 1.5  2004/07/06 15:31:06  lebedev
 * pcassay and pcsubstance object libs added
 *
 * Revision 1.4  2004/06/25 17:56:53  lebedev
 * Exclude gui/core/ProjectDescr.cpp from the build
 *
 * Revision 1.3  2004/06/25 15:15:38  lebedev
 * Some unnessesary debug traces removed
 *
 * Revision 1.1  2004/06/23 17:09:52  lebedev
 * Initial revision
 * ===========================================================================
 *)