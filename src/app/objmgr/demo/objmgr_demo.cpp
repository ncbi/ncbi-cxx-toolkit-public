/*  $Id$
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
* Author:  Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Examples of using the C++ object manager
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/random_gen.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/prefetch_actions.hpp>
#include <objmgr/table_field.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_seqid.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>

#ifdef HAVE_BDB
#  define HAVE_LDS 1
#elif defined(HAVE_LDS)
#  undef HAVE_LDS
#endif

#ifdef HAVE_LDS
#  include <objtools/data_loaders/lds/lds_dataloader.hpp>
#  include <objtools/lds/lds_manager.hpp>
#endif

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CDemoApp::Init(void)
{
    CONNECT_Init(&GetConfig());

    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddOptionalKey("gi", "SeqEntryID",
                             "GI id of the Seq-Entry to fetch",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("id", "SeqEntryID",
                             "Seq-id of the Seq-Entry to fetch",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("asn_id", "SeqEntryID",
                             "ASN.1 of Seq-id of the Seq-Entry to fetch",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("file", "SeqEntryFile",
                             "file with Seq-entry to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("bfile", "SeqEntryFile",
                             "file with Seq-entry to load (binary ASN.1)",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);
    arg_desc->AddOptionalKey("annot_file", "SeqAnnotFile",
                             "file with Seq-annot to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("annot_bfile", "SeqAnnotFile",
                             "file with Seq-annot to load (binary ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("count", "RepeatCount",
                            "repeat test work RepeatCount times",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("pause", "Pause",
                            "pause between tests in seconds",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("resolve", "ResolveMethod",
                            "Method of segments resolution",
                            CArgDescriptions::eString, "tse");
    arg_desc->SetConstraint("resolve",
                            &(*new CArgAllow_Strings,
                              "none", "tse", "all"));
    arg_desc->AddDefaultKey("missing", "UnresolvableIdMethod",
                            "Method of treating unresolvable ids",
                            CArgDescriptions::eString, "ignore");
    arg_desc->SetConstraint("missing",
                            &(*new CArgAllow_Strings,
                            "ignore", "search", "fail"));
    arg_desc->AddFlag("limit_tse", "Limit annotations from sequence TSE only");
    arg_desc->AddFlag("externals", "Search for external features only");

    arg_desc->AddOptionalKey("loader", "Loader",
                             "Use specified GenBank loader readers (\"-\" means no GenBank",
                             CArgDescriptions::eString);
#ifdef HAVE_LDS
    arg_desc->AddOptionalKey("lds_dir", "LDSDir",
                             "Use local data storage loader from the specified firectory",
                             CArgDescriptions::eString);
#endif
    arg_desc->AddOptionalKey("blast", "Blast",
                             "Use BLAST data loader from the specified DB",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("blast_type", "BlastType",
                             "Use BLAST data loader type (default: eUnknown)",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("blast_type",
                            &(*new CArgAllow_Strings,
                              "protein", "p", "nucleotide", "n"));

    arg_desc->AddFlag("seq_map", "scan SeqMap on full depth");
    arg_desc->AddFlag("seg_labels", "get labels of all segments in Delta");
    arg_desc->AddFlag("whole_sequence", "load whole sequence");
    arg_desc->AddFlag("whole_tse", "perform some checks on whole TSE");
    arg_desc->AddFlag("print_tse", "print TSE with sequence");
    arg_desc->AddFlag("print_seq", "print sequence");
    arg_desc->AddOptionalKey("desc_type", "DescType",
                             "look only descriptors of specified type",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("print_descr", "print all found descriptors");
    arg_desc->AddFlag("print_cds", "print CDS");
    arg_desc->AddFlag("print_features", "print all found features");
    arg_desc->AddFlag("print_mapper",
                      "print retult of CSeq_loc_Mapper "
                      "(when -print_features is set)");
    arg_desc->AddFlag("only_features", "do only one scan of features");
    arg_desc->AddFlag("by_product", "Search features by their product");
    arg_desc->AddFlag("count_types",
                      "print counts of different feature types");
    arg_desc->AddFlag("count_subtypes",
                      "print counts of different feature subtypes");
    arg_desc->AddOptionalKey("range_from", "RangeFrom",
                             "features starting at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("range_to", "RangeTo",
                             "features ending at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("range_loc", "RangeLoc",
                             "features on this Seq-loc in ASN.1 text format",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("overlap", "Overlap",
                            "Method of overlap location check",
                            CArgDescriptions::eString, "totalrange");
    arg_desc->SetConstraint("overlap",
                            &(*new CArgAllow_Strings,
                              "totalrange", "intervals"));
                             
    arg_desc->AddFlag("get_mapped_location", "get mapped location");
    arg_desc->AddFlag("get_original_feature", "get original location");
    arg_desc->AddFlag("get_mapped_feature", "get mapped feature");
    arg_desc->AddFlag("skip_alignments", "do not search for alignments");
    arg_desc->AddFlag("print_alignments", "print all found Seq-aligns");
    arg_desc->AddFlag("get_mapped_alignments", "get mapped alignments");
    arg_desc->AddFlag("reverse", "reverse order of features");
    arg_desc->AddFlag("no_sort", "do not sort features");
    arg_desc->AddDefaultKey("max_feat", "MaxFeat",
                            "Max number of features to iterate",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("depth", "depth",
                            "Max depth of segments to iterate",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddFlag("adaptive", "Use adaptive depth of segments");
    arg_desc->AddFlag("exact_depth", "Use exact depth of segments");
    arg_desc->AddFlag("unnamed",
                      "include features from unnamed Seq-annots");
    arg_desc->AddOptionalKey("named", "NamedAnnots",
                             "include features from named Seq-annots "
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("allnamed",
                      "include features from all named Seq-annots");
    arg_desc->AddFlag("nosnp",
                      "exclude snp features - only unnamed Seq-annots");
    arg_desc->AddOptionalKey("exclude_named", "ExcludeNamedAnnots",
                             "exclude features from named Seq-annots"
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("noexternal",
                      "include external annotations");
    arg_desc->AddOptionalKey("feat_type", "FeatType",
                             "Type of features to select",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("feat_subtype", "FeatSubType",
                            "Subtype of features to select",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(CSeqFeatData::eSubtype_any));
    arg_desc->AddOptionalKey("feat_id", "FeatId",
                             "Feat-id of features to search",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("used_memory_check", "exit(0) after loading sequence");
    arg_desc->AddFlag("reset_scope", "reset scope before exiting");
    arg_desc->AddFlag("modify", "try to modify Bioseq object");
    arg_desc->AddOptionalKey("table_field_name", "table_field_name",
                             "Table Seq-feat field name to retrieve",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("table_field_id", "table_field_id",
                             "Table Seq-feat field id to retrieve",
                             CArgDescriptions::eInteger);

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


extern CAtomicCounter newCObjects;


template<class C>
typename C::E_Choice GetVariant(const CArgValue& value)
{
    typedef typename C::E_Choice E_Choice;
    if ( !value ) {
        return C::e_not_set;
    }
    for ( int e = C::e_not_set; e < C::e_MaxChoice; ++e ) {
        if ( C::SelectionName(E_Choice(e)) == value.AsString() ) {
            return E_Choice(e);
        }
    }
    return E_Choice(NStr::StringToInt(value.AsString()));
}


int test1()
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();
    ifstream gi_in("prot_gis.txt");
    int gi;
    int seq_count = 0;
    int prot_count = 0;
    int gene_count = 0;
    int cdr_count = 0;
    vector<int> gis;
    vector<CSeq_id_Handle> ids;
    while ( gi_in >> gi ) {
        gis.push_back(gi);
        ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
    }
    
    CRef<CPrefetchManager> prefetch_manager;
    //prefetch_manager = new CPrefetchManager();
    CRef<CPrefetchSequence> prefetch;
    if ( prefetch_manager ) {
        // Initialize prefetch token;
        prefetch = new CPrefetchSequence
            (*prefetch_manager,
             new CPrefetchBioseqActionSource(CScopeSource(scope), ids));
    }

    ITERATE ( vector<CSeq_id_Handle>, id, ids ) {
        gi = id->GetGi();
        NcbiCout << "Loading gi "<<gi << NcbiEndl;
        CBioseq_Handle bh;
        if ( prefetch ) {
            bh = CStdPrefetch::GetBioseqHandle(prefetch->GetNextToken());
        }
        else {
            bh = scope.GetBioseqHandle(*id);
        }
        if ( !bh ) {
            ERR_POST("Cannot load gi "<<gi);
            continue;
        }
        CConstRef<CSeq_entry> core = bh.GetTSE_Handle().GetTSECore();
        if ( core->IsSeq() ) {
            NcbiCout << gi << " seq" << NcbiEndl;
        }
        else if ( !core->GetSet().IsSetId() ) {
            NcbiCout << gi << " set" << NcbiEndl;
        }
        else {
            NcbiCout << gi << " split" << NcbiEndl;
        }
        ++seq_count;
        if ( 0 ) {
            for ( CFeat_CI it(bh, CSeqFeatData::e_Prot); it; ++it ) {
                ++prot_count;
            }
            for ( CFeat_CI it(bh, SAnnotSelector(CSeqFeatData::e_Cdregion, true));
                  it; ++it ) {
                ++cdr_count;
                for ( CFeat_CI it2(scope, it->GetLocation(), CSeqFeatData::e_Gene);
                      it2; ++it2 ) {
                    ++gene_count;
                }
            }
        }
    }
    NcbiCout << "Total sequences: "<<seq_count
             << " prot: "<<prot_count
             << " gene: "<<gene_count
             << " cdr: "<<cdr_count
             << NcbiEndl;
    return 0;
}


int test2()
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();
    CSeq_id id("NT_029998.7");
    CBioseq_Handle bh = scope.GetBioseqHandle(id);
    CSeqMap& sm = bh.GetEditHandle().SetSeqMap();
    CSeqMap_CI it = sm.FindSegment(72470, &scope);
    for ( int i = 0; i < 3; ++i ) {
        NcbiCout << "Old seg: " << it.GetPosition() << " " << it.GetLength()
                 << NcbiEndl;
        NcbiCout << "Old len: " << bh.GetBioseqLength() << " "
                 << sm.GetLength(&scope)
                 << NcbiEndl;
        it = sm.RemoveSegment(it);
        NcbiCout << "New seg: " << it.GetPosition() << " " << it.GetLength()
                 << NcbiEndl;
        NcbiCout << "New len: " << bh.GetBioseqLength() << " "
                 << sm.GetLength(&scope)
                 << NcbiEndl;
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject()
                 << NcbiEndl;
    }
    return 0;
}


CNcbiOstream& operator<<(CNcbiOstream& out, const vector<char>& v)
{
    out << '\'';
    ITERATE ( vector<char>, i, v ) {
        int c = *i & 255;
        for ( int j = 0; j < 2; ++j ) {
            out << "0123456789ABCDEF"[(c>>4)&15];
            c <<= 4;
        }
    }
    out << "\'H";
    return out;
}


int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id;
    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        id.Reset(new CSeq_id);
        id->SetGi(gi);
    }
    else if ( args["id"] ) {
        id.Reset(new CSeq_id(args["id"].AsString()));
        NcbiCout << MSerial_AsnText << *id;
    }
    else if ( args["asn_id"] ) {
        id.Reset(new CSeq_id);
        string text = args["asn_id"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "Seq-id ::= " + text;
        }
        CNcbiIstrstream ss(text.c_str());
        ss >> MSerial_AsnText >> *id;
    }
    else {
        return test2();
        ERR_POST(Fatal << "One of -gi, -id or -asn_id arguments is required");
    }

    SAnnotSelector::EResolveMethod resolve = SAnnotSelector::eResolve_TSE;
    if ( args["resolve"].AsString() == "all" )
        resolve = SAnnotSelector::eResolve_All;
    if ( args["resolve"].AsString() == "none" )
        resolve = SAnnotSelector::eResolve_None;
    if ( args["resolve"].AsString() == "tse" )
        resolve = SAnnotSelector::eResolve_TSE;
    SAnnotSelector::EUnresolvedFlag missing = SAnnotSelector::eIgnoreUnresolved;
    if ( args["missing"].AsString() == "ignore" )
        missing = SAnnotSelector::eIgnoreUnresolved;
    if ( args["missing"].AsString() == "search" )
        missing = SAnnotSelector::eSearchUnresolved;
    if ( args["missing"].AsString() == "fail" )
        missing = SAnnotSelector::eFailUnresolved;
    bool externals_only = args["externals"];
    bool limit_tse = args["limit_tse"];

    int repeat_count = args["count"].AsInteger();
    int pause = args["pause"].AsInteger();
    bool only_features = args["only_features"];
    bool by_product = args["by_product"];
    bool count_types = args["count_types"];
    bool count_subtypes = args["count_subtypes"];
    bool print_tse = args["print_tse"];
    bool print_seq = args["print_seq"];
    bool print_descr = args["print_descr"];
    CSeqdesc::E_Choice desc_type =
        GetVariant<CSeqdesc>(args["desc_type"]);
    bool print_cds = args["print_cds"];
    bool print_features = args["print_features"];
    bool print_mapper = args["print_mapper"];
    bool get_mapped_location = args["get_mapped_location"];
    bool get_original_feature = args["get_original_feature"];
    bool get_mapped_feature = args["get_mapped_feature"];
    bool print_alignments = args["print_alignments"];
    bool skip_alignments = args["skip_alignments"];
    bool get_mapped_alignments = args["get_mapped_alignments"];
    SAnnotSelector::ESortOrder order =
        args["reverse"] ?
        SAnnotSelector::eSortOrder_Reverse : SAnnotSelector::eSortOrder_Normal;
    if ( args["no_sort"] )
        order = SAnnotSelector::eSortOrder_None;
    int max_feat = args["max_feat"].AsInteger();
    int depth = args["depth"].AsInteger();
    bool adaptive = args["adaptive"];
    bool exact_depth = args["exact_depth"];
    CSeqFeatData::E_Choice feat_type =
        GetVariant<CSeqFeatData>(args["feat_type"]);
    CSeqFeatData::ESubtype feat_subtype =
        CSeqFeatData::ESubtype(args["feat_subtype"].AsInteger());
    bool nosnp = args["nosnp"];
    bool include_unnamed = args["unnamed"];
    bool include_allnamed = args["allnamed"];
    bool noexternal = args["noexternal"];
    bool whole_tse = args["whole_tse"];
    bool whole_sequence = args["whole_sequence"];
    bool used_memory_check = args["used_memory_check"];
    bool get_synonyms = false;
    bool get_ids = true;
    bool get_blob_id = true;
    set<string> include_named;
    if ( args["named"] ) {
        string names = args["named"].AsString();
        size_t comma_pos;
        while ( (comma_pos = names.find(',')) != NPOS ) {
            include_named.insert(names.substr(0, comma_pos));
            names.erase(0, comma_pos+1);
        }
        include_named.insert(names);
    }
    set<string> exclude_named;
    if ( args["exclude_named"] ) {
        string names = args["exclude_named"].AsString();
        size_t comma_pos;
        while ( (comma_pos = names.find(',')) != NPOS ) {
            exclude_named.insert(names.substr(0, comma_pos));
            names.erase(0, comma_pos+1);
        }
        exclude_named.insert(names);
    }
    bool scan_seq_map = args["seq_map"];
    bool get_seg_labels = args["seg_labels"];

    vector<int> types_counts, subtypes_counts;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    CRef<CGBDataLoader> gb_loader;
    bool other_loaders = false;
    if ( args["loader"] ) {
        string genbank_readers = args["loader"].AsString();
        if ( genbank_readers != "-" ) {
            // Create genbank data loader and register it with the OM.
            // The last argument "eDefault" informs the OM that the loader
            // must be included in scopes during the CScope::AddDefaults() call
            gb_loader = CGBDataLoader::RegisterInObjectManager
                (*pOm, genbank_readers).GetLoader();
        }
    }
    else {
        gb_loader = CGBDataLoader::RegisterInObjectManager(*pOm).GetLoader();
    }
#ifdef HAVE_LDS
    if ( args["lds_dir"] ) {
        string lds_dir = args["lds_dir"].AsString();
        CLDS_Manager::ERecurse recurse = CLDS_Manager::eRecurseSubDirs;
        CLDS_Manager::EComputeControlSum control_sum =
            CLDS_Manager::eComputeControlSum;
        auto_ptr<CLDS_Database> lds_db;
        CLDS_Manager manager(lds_dir);

        try {
            lds_db.reset(manager.ReleaseDB());
        } catch(CBDB_ErrnoException&) {
            manager.Index(recurse, control_sum);
            lds_db.reset(manager.ReleaseDB());
        }

        CLDS_DataLoader::RegisterInObjectManager(*pOm, *lds_db,
                                                 CObjectManager::eDefault);
        other_loaders = true;
        lds_db.release();
    }
#endif
    if ( args["blast"] || args["blast_type"] ) {
        string db;
        if ( args["blast"] ) {
            db = args["blast"].AsString();
        }
        else {
            db = "nr";
        }
        CBlastDbDataLoader::EDbType type = CBlastDbDataLoader::eUnknown;
        if ( args["blast_type"] ) {
            string s = args["blast_type"].AsString();
            if ( s.size() > 0 && s[0] == 'p' ) {
                type = CBlastDbDataLoader::eProtein;
            }
            else if ( s.size() > 0 && s[0] == 'n' ) {
                type = CBlastDbDataLoader::eNucleotide;
            }
        }
        CBlastDbDataLoader::RegisterInObjectManager
            (*pOm, db, type, CObjectManager::eDefault, 88);
        other_loaders = true;
    }

    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        CScope  scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        NcbiCout << " gi "<<gi<<" -> acc "<<
            sequence::GetAccessionForGi(gi,scope,
                                        sequence::eWithoutAccessionVersion)
                 << NcbiEndl;
    }

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    {{
        CNcbiIfstream in("sparse_test.asn");
        CSeq_loc loc1, loc2;
        CSeq_align aln;
        in >> MSerial_AsnText >> loc1;
        in >> MSerial_AsnText >> loc2;
        in >> MSerial_AsnText >> aln;
        //CSeq_loc_Mapper mapper1(aln, 0, &scope, CSeq_loc_Mapper::fAlign_Sparse_ToFirst);
        //CSeq_loc_Mapper mapper2(aln, 0, &scope, CSeq_loc_Mapper::fAlign_Sparse_ToSecond);
        //CRef<CSeq_loc> loc1m = mapper1.Map(loc1);
        //CRef<CSeq_loc> loc2m = mapper2.Map(loc2);
        //cout << MSerial_AsnText << *loc1m;
        //cout << MSerial_AsnText << *loc2m;
        CSeq_loc_Mapper mapper(loc1, loc2, &scope);
        CRef<CSeq_align> aln2 = mapper.Map(aln);
        cout << MSerial_AsnText << *aln2;
        return 0;
    }}

    CSeq_entry_Handle added_entry;
    CSeq_annot_Handle added_annot;
    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["file"].AsInputFile() >> MSerial_AsnText >> *entry;
        if ( used_memory_check ) {
            exit(0);
        }
        added_entry = scope.AddTopLevelSeqEntry(*entry);
    }
    if ( args["bfile"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["bfile"].AsInputFile() >> MSerial_AsnBinary >> *entry;
        added_entry = scope.AddTopLevelSeqEntry(*entry);
    }
    if ( args["annot_file"] ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        args["annot_file"].AsInputFile() >> MSerial_AsnText >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }
    if ( args["annot_bfile"] ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        args["annot_bfile"].AsInputFile() >> MSerial_AsnBinary >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    if ( get_ids ) {
        NcbiCout << "Ids:" << NcbiEndl;
        //scope.GetBioseqHandle(idh);
        vector<CSeq_id_Handle> ids = scope.GetIds(idh);
        ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
            NcbiCout << "    " << it->AsString() << NcbiEndl;
        }
    }
    if ( get_blob_id && gb_loader && !other_loaders ) {
        try {
            CDataLoader::TBlobId blob_id = gb_loader->GetBlobId(idh);
            if ( !blob_id ) {
                ERR_POST("Cannot find blob id of "<<id->AsFastaString());
            }
            else {
                NcbiCout << "Resolved: "<<id->AsFastaString()<<
                    " -> "<<blob_id.ToString()<<NcbiEndl;
            }
        }
        catch ( CException& exc ) {
            ERR_POST("Cannot blob id of "<<id->AsFastaString()<<": "<<
                     exc.GetMsg());
        }
    }

    // Get bioseq handle for the seq-id. Most of requests will use this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(idh);
    if ( handle.GetState() ) {
        // print blob state:
        NcbiCout << "Bioseq state: 0x" << hex << handle.GetState() << dec
                 << NcbiEndl;
    }
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found.");
    }
    if ( get_synonyms ) {
        NcbiCout << "Synonyms:" << NcbiEndl;
        CConstRef<CSynonymsSet> syns = scope.GetSynonyms(handle);
        ITERATE ( CSynonymsSet, it, *syns ) {
            CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(it);
            NcbiCout << "    " << idh.AsString() << NcbiEndl;
        }
    }

    if ( print_tse ) {
        CConstRef<CSeq_entry> entry =
            handle.GetTopLevelEntry().GetCompleteSeq_entry();
        //CConstRef<CBioseq> entry =
        //    handle.GetEditHandle().GetCompleteObject();
        NcbiCout << "-------------------- TSE --------------------\n";
        NcbiCout << MSerial_AsnText << *entry << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }
    if ( print_seq ) {
        handle.GetEditHandle();
        NcbiCout << "-------------------- SEQ --------------------\n";
        NcbiCout << MSerial_AsnText << *handle.GetCompleteObject() << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }

    CRef<CSeq_id> search_id = id;
    CRef<CSeq_loc> whole_loc(new CSeq_loc);
    // No region restrictions -- the whole bioseq is used:
    whole_loc->SetWhole(*search_id);
    CRef<CSeq_loc> range_loc;
    TSeqPos range_from, range_to;
    if ( args["range_from"] || args["range_to"] ) {
        if ( args["range_from"] ) {
            range_from = args["range_from"].AsInteger();
        }
        else {
            range_from = 0;
        }
        if ( args["range_to"] ) {
            range_to = args["range_to"].AsInteger();
        }
        else {
            range_to = handle.GetBioseqLength()-1;
        }
        range_loc.Reset(new CSeq_loc);
        range_loc->SetInt().SetId(*search_id);
        range_loc->SetInt().SetFrom(range_from);
        range_loc->SetInt().SetTo(range_to);
    }
    else {
        range_from = range_to = 0;
        range_loc = whole_loc;
    }
    if ( args["range_loc"] ) {
        CNcbiIstrstream in(args["range_loc"].AsString().c_str());
        in >> MSerial_AsnText >> *range_loc;
    }
    SAnnotSelector::EOverlapType overlap = SAnnotSelector::eOverlap_TotalRange;
    if ( args["overlap"].AsString() == "totalrange" )
        overlap = SAnnotSelector::eOverlap_TotalRange;
    if ( args["overlap"].AsString() == "intervals" )
        overlap = SAnnotSelector::eOverlap_Intervals;
    string table_field_name;
    if ( args["table_field_name"] )
        table_field_name = args["table_field_name"].AsString();
    int table_field_id = -1;
    if ( args["table_field_id"] )
        table_field_id = args["table_field_id"].AsInteger();

    handle.Reset();
    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c && pause ) {
            SleepSec(pause);
        }
        
        // get handle again, check for scope TSE locking
        handle = scope.GetBioseqHandle(idh);
        //scope.RemoveFromHistory(handle.GetTSE_Handle()); break;

        if ( get_seg_labels ) {
            TSeqPos range_length =
                range_to == 0? kInvalidSeqPos: range_to - range_from + 1;
            CSeqMap::TFlags flags = CSeqMap::fDefaultFlags;
            if ( exact_depth ) {
                flags |= CSeqMap::fFindExactLevel;
            }
            const CSeqMap& seq_map = handle.GetSeqMap();
            CSeqMap::const_iterator seg =
                seq_map.ResolvedRangeIterator(&scope,
                                              range_from,
                                              range_length,
                                              eNa_strand_plus,
                                              1,
                                              flags);
            for ( ; seg; ++seg ) {
                if ( seg.GetType() == CSeqMap::eSeqRef ) {
                    string label = scope.GetLabel(seg.GetRefSeqid());
                    NcbiCout << "Label(" << seg.GetRefSeqid().AsString()
                             << ") = " << label << NcbiEndl;
                }
            }
        }

        string sout;
        int count;
        if ( !only_features ) {
            // List other sequences in the same TSE
            if ( whole_tse ) {
                NcbiCout << "TSE sequences:" << NcbiEndl;
                for ( CBioseq_CI bit(handle.GetTopLevelEntry()); bit; ++bit) {
                    NcbiCout << "    "<<bit->GetSeqId()->DumpAsFasta()<<
                        NcbiEndl;
                }
            }
            else if ( 0 ) {
                count = 0;
                for ( CBioseq_CI bit(handle.GetTopLevelEntry()); bit; ++bit) {
                    ++count;
                }
                NcbiCout << "TSE sequences:\t" << count << NcbiEndl;
            }

            // Get the bioseq
            CConstRef<CBioseq> bioseq(handle.GetBioseqCore());
            // -- use the bioseq: print the first seq-id
            NcbiCout << "First ID = " <<
                (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

            // Get the sequence using CSeqVector. Use default encoding:
            // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
            CSeqVector seq_vect =
                handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            // -- use the vector: print length and the first 10 symbols
            NcbiCout << "Sequence: length=" << seq_vect.size();
            if (seq_vect.CanGetRange(0, seq_vect.size() - 1)) {
                NcbiCout << " data=";
                sout.erase();
                for (TSeqPos i = 0; (i < seq_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += seq_vect[i];
                }
                NcbiCout << NStr::PrintableString(sout) << NcbiEndl;

                if ( whole_sequence ) {
                    sout.erase();
                    size_t size = seq_vect.size();
                    seq_vect.GetSeqData(0, size, sout);
                    NcbiCout << " data["<<size<<"] = ";
                    size_t start = min(size, size_t(10));
                    size_t end = min(size-start, size_t(10));
                    NcbiCout << NStr::PrintableString(sout.substr(0, start));
                    if ( start + end != size )
                        NcbiCout << "..";
                    NcbiCout << NStr::PrintableString(sout.substr(size-end,
                                                                  end));
                    NcbiCout << NcbiEndl;
                }
            }
            else {
                NcbiCout << " data unavailable" << NcbiEndl;
            }
            // CSeq_descr iterator: iterates all descriptors starting
            // from the bioseq and going the seq-entries tree up to the
            // top-level seq-entry.
            count = 0;
            for (CSeqdesc_CI desc_it(handle, desc_type); desc_it; ++desc_it) {
                if ( print_descr ) {
                    NcbiCout << "\n" << MSerial_AsnText << *desc_it;
                }
                count++;
            }
            cout << "\n";
            NcbiCout << "Seqdesc count (sequence):\t" << count << NcbiEndl;

            count = 0;
            for ( CSeq_annot_CI ai(handle.GetParentEntry()); ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (recursive):\t"
                     << count << NcbiEndl;
            
            count = 0;
            for ( CSeq_annot_CI ai(handle.GetParentEntry(),
                                   CSeq_annot_CI::eSearch_entry);
                  ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (non-recurs):\t"
                     << count << NcbiEndl;

            if ( whole_tse ) {
                count = 0;
                for ( CSeq_annot_CI ai(handle); ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (up to TSE):\t"
                         << count << NcbiEndl;

                count = 0;
                for (CSeq_annot_CI ai(handle.GetTopLevelEntry()); ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (TSE, recursive):\t"
                         << count << NcbiEndl;
                
                count = 0;
                for (CSeq_annot_CI ai(handle.GetTopLevelEntry(),
                                      CSeq_annot_CI::eSearch_entry);
                     ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (TSE, non-recurs):\t"
                         << count << NcbiEndl;
            }
        }

        // CSeq_feat iterator: iterates all features which can be found in the
        // current scope including features from all TSEs.
        count = 0;
        // Create CFeat_CI using the current scope and location.
        // No feature type restrictions.
        SAnnotSelector base_sel;
        base_sel
            .SetResolveMethod(resolve)
            .SetOverlapType(overlap)
            .SetSortOrder(order)
            .SetMaxSize(max_feat)
            .SetResolveDepth(depth)
            .SetAdaptiveDepth(adaptive)
            .SetExactDepth(exact_depth)
            .SetUnresolvedFlag(missing);
        if ( externals_only ) {
            base_sel.SetSearchExternal(handle);
        }
        if ( limit_tse ) {
            if ( added_annot ) {
                base_sel.SetLimitSeqAnnot(added_annot);
            }
            else if ( added_entry ) {
                base_sel.SetLimitSeqEntry(added_entry);
            }
            else {
                base_sel.SetLimitTSE(handle.GetTopLevelEntry());
            }
        }
        if ( include_allnamed ) {
            base_sel.SetAllNamedAnnots();
        }
        if ( include_unnamed ) {
            base_sel.AddUnnamedAnnots();
        }
        ITERATE ( set<string>, it, include_named ) {
            base_sel.AddNamedAnnots(*it);
        }
        if ( nosnp ) {
            base_sel.ExcludeNamedAnnots("SNP");
        }
        ITERATE ( set<string>, it, exclude_named ) {
            base_sel.ExcludeNamedAnnots(*it);
        }
        if ( noexternal ) {
            base_sel.SetExcludeExternal();
        }
        string sel_msg = "any";
        if ( feat_type != CSeqFeatData::e_not_set ) {
            base_sel.SetFeatType(feat_type);
            sel_msg = "req";
        }
        if ( feat_subtype != CSeqFeatData::eSubtype_any ) {
            base_sel.SetFeatSubtype(feat_subtype);
            sel_msg = "req";
        }
        base_sel.SetByProduct(by_product);

        typedef int TTableField;
        auto_ptr< CTableFieldHandle<TTableField> > table_field;
        if ( table_field_id >= 0 ) {
            table_field.reset(new CTableFieldHandle<TTableField>(CSeqTable_column_info::EField_id(table_field_id)));
        }
        else if ( !table_field_name.empty() ) {
            table_field.reset(new CTableFieldHandle<TTableField>(table_field_name));
        }

        {{
            if ( count_types ) {
                types_counts.assign(CSeqFeatData::e_MaxChoice, 0);
            }
            if ( count_subtypes ) {
                subtypes_counts.assign(CSeqFeatData::eSubtype_max+1, 0);
            }
            CRef<CSeq_loc_Mapper> mapper;
            if ( print_features && print_mapper ) {
                if ( 0 ) {
                    CRef<CBioseq> seq(new CBioseq);
                    CRef<CSeq_id> id(new CSeq_id);
                    id->SetGi(1);
                    seq->SetId().push_back(id);
                    TSeqPos len = handle.GetBioseqLength();
                    seq->SetInst().SetLength(len);
                    seq->SetInst().SetMol(handle.GetSequenceType());
                    scope.AddBioseq(*seq);

                    CRef<CSeq_loc> from_loc(new CSeq_loc);
                    from_loc->SetInt().SetFrom(0);
                    from_loc->SetInt().SetTo  (len-1);
                    //from_loc->SetId(*handle.GetSeqId());
                    ITERATE ( CBioseq_Handle::TId, it, handle.GetId() ) {
                        if ( *it != idh ) {
                            from_loc->SetId(*it->GetSeqId());
                            break;
                        }
                    }
                
                    CRef<CSeq_loc> to_loc(new CSeq_loc);
                    to_loc->SetInt().SetFrom(0);
                    to_loc->SetInt().SetTo(len-1);
                    to_loc->SetId(*id);

                    NcbiCout << "Mapping from " << MSerial_AsnText << *from_loc <<
                        " to " << MSerial_AsnText << *to_loc;
                    mapper.Reset(new CSeq_loc_Mapper(*from_loc,
                                                     *to_loc,
                                                     &scope));
                }
                else {
                    mapper.Reset(new CSeq_loc_Mapper(handle,
                                                     CSeq_loc_Mapper::eSeqMap_Up));
                }
            }
            if ( args["feat_id"] ) {
                int feat_id = args["feat_id"].AsInteger();
                vector<CSeq_feat_Handle> feats;
                CTSE_Handle tse = handle.GetTSE_Handle();
                for ( int t = 0; t < 4; ++t ) {
                    switch ( t ) {
                    case 0:
                        NcbiCout << "Features with id "
                                 << feat_id << " +type:";
                        feats = tse.GetFeaturesWithId(feat_type, feat_id);
                        break;
                    case 1:
                        NcbiCout << "Features with id "
                                 << feat_id << " +subtype:";
                        feats = tse.GetFeaturesWithId(feat_subtype, feat_id);
                        break;
                    case 2:
                        NcbiCout << "Features with xref "
                                 << feat_id << " +type:";
                        feats = tse.GetFeaturesWithXref(feat_type, feat_id);
                        break;
                    case 3:
                        NcbiCout << "Features with xref "
                                 << feat_id << " +subtype:";
                        feats = tse.GetFeaturesWithXref(feat_subtype, feat_id);
                        break;
                    }
                    if ( print_features ) {
                        NcbiCout << "\n";
                        ITERATE ( vector<CSeq_feat_Handle>, it, feats ) {
                            NcbiCout << MSerial_AsnText << *it->GetSeq_feat();
                        }
                    }
                    else {
                        NcbiCout << " " << feats.size() << NcbiEndl;
                    }
                }
            }
            for ( CFeat_CI it(scope, *range_loc, base_sel); it;  ++it) {
                if ( count_types ) {
                    ++types_counts[it->GetFeatType()];
                }
                if ( count_subtypes ) {
                    ++subtypes_counts[it->GetFeatSubtype()];
                }
                ++count;
                if ( get_mapped_location )
                    it->GetLocation();
                if ( get_original_feature )
                    it->GetOriginalFeature();
                if ( get_mapped_feature )
                    it->GetMappedFeature();

                if ( table_field.get() &&
                     it->GetSeq_feat_Handle().IsTableFeat() ) {
                    TTableField value;
                    if ( table_field->TryGet(it, value) ) {
                        NcbiCout << "table field: " << value << NcbiEndl;
                    }
                }
                
                // Get seq-annot containing the feature
                if ( print_features ) {
                    NcbiCout << "Feature:";
                    if ( it->IsSetPartial() ) {
                        NcbiCout << " partial = " << it->GetPartial();
                    }
                    NcbiCout << "\n" <<
                        MSerial_AsnText << it->GetMappedFeature();
                    if ( 1 ) {
                        NcbiCout << "Original location:";
                        if ( it->GetOriginalFeature().IsSetPartial() ) {
                            NcbiCout << " partial = " <<
                                it->GetOriginalFeature().GetPartial();
                        }
                        NcbiCout << "\n" <<
                            MSerial_AsnText <<
                            it->GetOriginalFeature().GetLocation();
                        if ( mapper ) {
                            NcbiCout << "Mapped orig location:\n" <<
                                MSerial_AsnText <<
                                *mapper->Map(it->GetOriginalFeature()
                                             .GetLocation());
                            NcbiCout << "Mapped iter location:\n"<<
                                MSerial_AsnText <<
                                *mapper->Map(it->GetLocation());
                        }
                    }
                    else {
                        NcbiCout << "Location:\n" <<
                            MSerial_AsnText << it->GetLocation();
                    }
                }

                if ( it->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA &&
                     it->IsSetProduct() ) {
                    using namespace sequence;
                    handle.GetEditHandle();
                    CSeq_id_Handle prod_idh =
                        GetIdHandle(it->GetProduct(), NULL);
                    NcbiCout << "mRNA product: " << prod_idh.AsString()
                             << NcbiEndl;
                    CBioseq_Handle bsh =
                        scope.GetBioseqHandleFromTSE(prod_idh, handle);
                    if ( bsh ) {
                        NcbiCout << "GetBestXxxForMrna: "
                                 << MSerial_AsnText
                                 << it->GetOriginalFeature()
                                 << NcbiEndl;
                        
                        CConstRef<CSeq_feat> gene =
                            GetBestGeneForMrna(it->GetOriginalFeature(),
                                               scope);
                        NcbiCout << "GetBestGeneForMrna: ";
                        if ( gene ) {
                            NcbiCout << MSerial_AsnText << *gene;
                        }
                        else {
                            NcbiCout << "null";
                        }
                        NcbiCout << NcbiEndl;
                        CConstRef<CSeq_feat> cds =
                            GetBestCdsForMrna(it->GetOriginalFeature(),
                                              scope);
                        NcbiCout << "GetBestCdsForMrna: ";
                        if ( cds ) {
                            NcbiCout << MSerial_AsnText << *cds;
                        }
                        else {
                            NcbiCout << "null";
                        }
                        NcbiCout << NcbiEndl;
                    }
                }
                if ( 0 && it->IsSetXref() ) {
                    ITERATE ( CSeq_feat::TXref, xit, it->GetXref() ) {
                        const CSeqFeatXref& xref = **xit;
                        if ( !xref.IsSetId() )
                            continue;
                        int id = xref.GetId().GetLocal().GetId();
                        NcbiCout << " xref: " << id << NcbiEndl;
                        CTSE_Handle tse = it->GetAnnot().GetTSE_Handle();
                        vector<CSeq_feat_Handle> ff =
                            tse.GetFeaturesWithId(CSeqFeatData::e_not_set, id);
                        ITERATE ( vector<CSeq_feat_Handle>, it, ff ) {
                            NcbiCout << MSerial_AsnText
                                     << *it->GetSeq_feat() << NcbiEndl;
                        }
                    }
                }

                CSeq_annot_Handle annot = it.GetAnnot();
                /*
                  const CSeq_id* mapped_id = 0;
                  it->GetLocation().CheckId(mapped_id);
                  _ASSERT(mapped_id);
                  _ASSERT(CSeq_id_Handle::GetHandle(*mapped_id) == idh);
                */
            }
            NcbiCout << "Feat count (loc range, " << sel_msg << "):\t"
                     << count << NcbiEndl;
            if ( count_types ) {
                ITERATE ( vector<int>, it, types_counts ) {
                    if ( *it ) {
                        CSeqFeatData::E_Choice type =
                            CSeqFeatData::E_Choice(it-types_counts.begin());
                        NcbiCout << "  type " <<
                            setw(2) << type <<
                            setw(10) << CSeqFeatData::SelectionName(type) <<
                            " : " << *it << NcbiEndl;
                    }
                }
            }
            if ( count_subtypes ) {
                ITERATE ( vector<int>, it, subtypes_counts ) {
                    if ( *it ) {
                        CSeqFeatData::ESubtype subtype =
                            CSeqFeatData::ESubtype(it-subtypes_counts.begin());
                        CSeqFeatData::E_Choice type =
                            CSeqFeatData::GetTypeFromSubtype(subtype);
                        NcbiCout << "  subtype " <<
                            setw(3) << subtype <<
                            setw(10) << CSeqFeatData::SelectionName(type) <<
                            " : " << *it << NcbiEndl;
                    }
                }
            }
        }}

        if ( !only_features ) {
            count = 0;
            // The same region, but restricted feature type:
            // searching for e_Cdregion features only. If the sequence is
            // segmented (constructed), search for features on the referenced
            // sequences in the same top level seq-entry, ignore far pointers.
            SAnnotSelector sel = base_sel;
            sel.SetFeatType(CSeqFeatData::e_Cdregion);
            size_t no_product_count = 0;
            for ( CFeat_CI it(scope, *range_loc, sel); it;  ++it ) {
                count++;
                // Get seq vector filtered with the current feature location.
                // e_ViewMerged flag forces each residue to be shown only once.
                CSeqVector cds_vect;
                if ( by_product ) {
                    cds_vect = CSeqVector(it->GetLocation(), scope,
                                          CBioseq_Handle::eCoding_Iupac);
                }
                else {
                    if ( it->IsSetProduct() ) {
                        cds_vect = CSeqVector(it->GetProduct(), scope,
                                              CBioseq_Handle::eCoding_Iupac);
                    }
                    else {
                        ++no_product_count;
                        /*
                          ERR_POST("Cdregion with no product");
                          NcbiCout << "Original location: " << MSerial_AsnText <<
                          it->GetOriginalFeature().GetLocation();
                        */
                        continue;
                    }
                }
                // Print first 10 characters of each cd-region
                if ( print_cds ) {
                    NcbiCout << "cds" << count <<
                        " len=" << cds_vect.size() << " data=";
                }
                if ( cds_vect.size() == 0 ) {
                    NcbiCout << "Zero size from: " << MSerial_AsnText <<
                        it->GetOriginalFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetMappedFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetLocation();

                    CSeqVector v2(it->GetLocation(), scope,
                                  CBioseq_Handle::eCoding_Iupac);
                    NcbiCout << v2.size() << NcbiEndl;
                    
                    const CSeq_id* mapped_id = 0;
                    it->GetMappedFeature().GetLocation().CheckId(mapped_id);
                    _ASSERT(mapped_id);
                    _ASSERT(by_product ||
                            CSeq_id_Handle::GetHandle(*mapped_id)==idh);
                }
                
                sout = "";
                for (TSeqPos i = 0; (i < cds_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += cds_vect[i];
                }
                if ( print_cds ) {
                    NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
                }
            }
            NcbiCout << "Feat count (loc range, cds):\t" << count << NcbiEndl;
            if ( no_product_count ) {
                NcbiCout << "*** no product on " << no_product_count << " cds"
                         << NcbiEndl;
            }
        }

        // Search features only in the TSE containing the target bioseq.
        // Since only one seq-id may be used as the target bioseq, the
        // iterator is constructed not from a seq-loc, but from a bioseq handle
        // and start/stop points on the bioseq.
        // If both start and stop are 0 the whole bioseq is used.
        // The last parameter may be used for type filtering.
        count = 0;
        for ( CFeat_CI it(scope, *range_loc, base_sel); it; ++it ) {
            count++;
        }
        NcbiCout << "Feat count (bh range, " << sel_msg << "):\t"
                 << count << NcbiEndl;

        if ( !only_features ) {
            if ( whole_tse ) {
                count = 0;
                for (CFeat_CI it(handle.GetTopLevelEntry(), base_sel);
                     it; ++it) {
                    count++;
                }
                NcbiCout << "Feat count        (TSE):\t" << count << NcbiEndl;
            }

            // The same way may be used to iterate aligns and graphs,
            // except that there is no type filter for both of them.
            count = 0;
            for ( CGraph_CI it(scope, *range_loc, base_sel); it;  ++it) {
                count++;
                // Get seq-annot containing the feature
                if ( get_mapped_location )
                    it->GetLoc();
                if ( get_original_feature )
                    it->GetOriginalGraph();
                if ( get_mapped_feature )
                    it->GetMappedGraph();
                if ( print_features ) {
                    NcbiCout << MSerial_AsnText <<
                        it->GetMappedGraph() << it->GetLoc();
                }
                CSeq_annot_Handle annot = it.GetAnnot();
            }
            NcbiCout << "Graph count (loc range):\t" << count << NcbiEndl;

            if ( !skip_alignments ) {
                count = 0;
                // Create CAlign_CI using the current scope and location.
                for (CAlign_CI it(scope, *range_loc, base_sel); it;  ++it) {
                    count++;
                    if ( get_mapped_alignments ) {
                        *it;
                    }
                    if ( print_alignments ) {
                        NcbiCout << MSerial_AsnText << *it;
                    }
                }
                NcbiCout << "Align count (loc range):\t" <<count<<NcbiEndl;
            }
        }

        if ( scan_seq_map ) {
            TSeqPos range_length =
                range_to == 0? kInvalidSeqPos: range_to - range_from + 1;
            TSeqPos actual_end =
                range_to == 0? handle.GetBioseqLength(): range_to + 1;
            TSeqPos actual_length = actual_end; actual_length -= range_from;
            const CSeqMap& seq_map = handle.GetSeqMap();
            NcbiCout << "Mol type: " << seq_map.GetMol() << NcbiEndl;
            for (size_t level = 0;  level < 5;  ++level) {
                NcbiCout << "Level " << level << NcbiEndl;
                TSeqPos total_length = 0;
                CSeqMap::TFlags flags = CSeqMap::fDefaultFlags;
                if ( exact_depth ) {
                    flags |= CSeqMap::fFindExactLevel;
                }
                CSeqMap::const_iterator seg =
                    seq_map.ResolvedRangeIterator(&scope,
                                                  range_from,
                                                  range_length,
                                                  eNa_strand_plus,
                                                  level,
                                                  flags);
                _ASSERT(level || seg.GetPosition() == range_from);
                for ( ;  seg;  ++seg ) {
                    NcbiCout << " @" << seg.GetPosition() << "-" <<
                        seg.GetEndPosition() << " +" <<
                        seg.GetLength() << ": ";
                    _ASSERT(seg.GetEndPosition()-seg.GetPosition() == seg.GetLength());
                    switch (seg.GetType()) {
                    case CSeqMap::eSeqRef:
                        NcbiCout << "ref: " <<
                            seg.GetRefSeqid().AsString() << " " <<
                            (seg.GetRefMinusStrand()? "minus ": "") <<
                            seg.GetRefPosition() << "-" <<
                            seg.GetRefEndPosition();
                        _ASSERT(seg.GetRefEndPosition()-seg.GetRefPosition() == seg.GetLength());
                        break;
                    case CSeqMap::eSeqData:
                        NcbiCout << "data: ";
                        break;
                    case CSeqMap::eSeqGap:
                        NcbiCout << "gap: ";
                        break;
                    case CSeqMap::eSeqEnd:
                        NcbiCout << "end: ";
                        _ASSERT("Unexpected END segment" && 0);
                        break;
                    default:
                        NcbiCout << "?: ";
                        _ASSERT("Unexpected segment type" && 0);
                        break;
                    }
                    total_length += seg.GetLength();
                    NcbiCout << NcbiEndl;
                }
                _ASSERT(level || total_length == actual_length);
                _ASSERT(seg.GetPosition() == actual_end);
                _ASSERT(seg.GetLength() == 0);
                TSeqPos new_length = 0;
                for ( --seg; seg; --seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    new_length += seg.GetLength();
                }
                _ASSERT(total_length == new_length);
                _ASSERT(level || seg.GetPosition() == range_from);
                _ASSERT(seg.GetLength() == 0);
                new_length = 0;
                for ( ++seg; seg; ++seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    new_length += seg.GetLength();
                }
                _ASSERT(total_length == new_length);
                _ASSERT(seg.GetPosition() == actual_end);
                _ASSERT(seg.GetLength() == 0);
            }
            CSeqMap::const_iterator begin = seq_map.begin(0);
            _ASSERT(begin.GetPosition() == 0);
            CSeqMap::const_iterator end = seq_map.end(0);
            _ASSERT(end.GetType() == CSeqMap::eSeqEnd);
            _ASSERT(end.GetPosition() == handle.GetBioseqLength());
            TSeqPos total_length = 0;
            for ( CSeqMap::const_iterator iter = begin; iter != end; ++iter ) {
                _ASSERT(iter.GetType() != CSeqMap::eSeqEnd);
                total_length += iter.GetLength();
            }
            _ASSERT(total_length == handle.GetBioseqLength());
            total_length = 0;
            for ( CSeqMap::const_iterator iter = end; iter != begin; ) {
                --iter;
                _ASSERT(iter.GetType() != CSeqMap::eSeqEnd);
                total_length += iter.GetLength();
            }
            _ASSERT(total_length == handle.GetBioseqLength());
        }

        if ( args["modify"] ) {
            //CTSE_Handle tse = handle.GetTSE_Handle();
            //CBioseq_EditHandle ebh = handle.GetEditHandle();
            CRef<CBioseq> newseq(new CBioseq);
            newseq->Assign(*handle.GetCompleteObject());
            CSeq_entry_Handle seh = handle.GetParentEntry();
            seh.GetEditHandle().SelectNone();
            seh.GetEditHandle().SelectSeq(*newseq);
        }
        if ( used_memory_check ) {
            if ( args["reset_scope"] ) {
                scope.ResetHistory();
                handle.Reset();
            }
            exit(0);
        }

        handle.Reset();
        scope.ResetHistory();
    }
    if ( args["modify"] ) {
        handle = scope.GetBioseqHandle(idh);
        CBioseq_EditHandle ebh = handle.GetEditHandle();
    }

    NcbiCout << "Done" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    int ret = CDemoApp().AppMain(argc, argv);
    NcbiCout << NcbiEndl;
    return ret;
}
