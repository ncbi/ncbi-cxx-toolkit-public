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
#include <objmgr/impl/synonyms.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

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

    CRef<CGBDataLoader> gb_loader;
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
                             CArgDescriptions::eInputFile);
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

    arg_desc->AddDefaultKey("loader", "Loader",
                            "Use specified loaders",
                            CArgDescriptions::eString, "");


    arg_desc->AddFlag("seq_map", "scan SeqMap on full depth");
    arg_desc->AddFlag("whole_sequence", "load whole sequence");
    arg_desc->AddFlag("whole_tse", "perform some checks on whole TSE");
    arg_desc->AddFlag("print_tse", "print TSE with sequence");
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
    arg_desc->AddFlag("used_memory_check", "exit(0) after loading sequence");
    arg_desc->AddFlag("reset_scope", "reset scope before exiting");

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
    if ( !args["loader"].AsString().empty() ) {
        string env = "GENBANK_LOADER_METHOD="+args["loader"].AsString();
        ::putenv(::strdup(env.c_str()));
    }
    bool limit_tse = args["limit_tse"];

    int repeat_count = args["count"].AsInteger();
    int pause = args["pause"].AsInteger();
    bool only_features = args["only_features"];
    bool by_product = args["by_product"];
    bool count_types = args["count_types"];
    bool count_subtypes = args["count_subtypes"];
    bool print_tse = args["print_tse"];
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

    vector<int> types_counts, subtypes_counts;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    if ( !gb_loader ) {
        // Create genbank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader
        // must be included in scopes during the CScope::AddDefaults() call
        gb_loader.Reset(CGBDataLoader::RegisterInObjectManager
                        (*pOm).GetLoader());
    }

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

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
        vector<CSeq_id_Handle> ids = scope.GetIds(idh);
        ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
            NcbiCout << "    " << it->AsString() << NcbiEndl;
        }
    }
    {{
        CDataLoader::TBlobId blob_id = gb_loader->GetBlobId(idh);
        if ( !blob_id ) {
            ERR_POST("Cannot resolve Seq-id "<<id->AsFastaString());
        }
        else {
            NcbiCout << "Resolved: "<<id->AsFastaString()<<
                " -> "<<gb_loader->GetBlobId(blob_id).ToString()<<NcbiEndl;
        }
    }}

    // Get bioseq handle for the seq-id. Most of requests will use this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(idh);
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found");
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
        NcbiCout << "-------------------- TSE --------------------\n";
        NcbiCout << MSerial_AsnText << *entry << '\n';
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

    handle.Reset();
    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c && pause ) {
            SleepSec(pause);
        }
        
        // get handle again, check for scope TSE locking
        handle = scope.GetBioseqHandle(idh);

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

        {{
            if ( count_types ) {
                types_counts.assign(CSeqFeatData::e_MaxChoice, 0);
            }
            if ( count_subtypes ) {
                subtypes_counts.assign(CSeqFeatData::eSubtype_max+1, 0);
            }
            CRef<CSeq_loc_Mapper> mapper;
            if ( print_features && print_mapper ) {
                mapper.Reset(new CSeq_loc_Mapper(handle,
                                                 CSeq_loc_Mapper::eSeqMap_Up));
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
                            NcbiCout << "Mapped location:";
                            NcbiCout << "\n" <<
                                MSerial_AsnText <<
                                *mapper->Map(it->GetOriginalFeature()
                                             .GetLocation());
                        }
                    }
                    else {
                        NcbiCout << "Location:\n" <<
                            MSerial_AsnText << it->GetLocation();
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
            for (size_t levels = 0;  levels < 4;  ++levels) {
                TSeqPos total_length = 0;
                CSeqMap::const_iterator seg =
                    seq_map.ResolvedRangeIterator(&scope,
                                                  range_from,
                                                  range_length,
                                                  eNa_strand_plus, levels);
                _ASSERT(seg.GetPosition() == range_from);
                for ( ;  seg;  ++seg ) {
                    switch (seg.GetType()) {
                    case CSeqMap::eSeqRef:
                        break;
                    case CSeqMap::eSeqData:
                    case CSeqMap::eSeqGap:
                        break;
                    case CSeqMap::eSeqEnd:
                        _ASSERT("Unexpected END segment" && 0);
                        break;
                    default:
                        _ASSERT("Unexpected segment type" && 0);
                        break;
                    }
                    total_length += seg.GetLength();
                }
                _ASSERT(total_length == actual_length);
                _ASSERT(seg.GetPosition() == actual_end);
                _ASSERT(seg.GetLength() == 0);
                total_length = 0;
                for ( --seg; seg; --seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    total_length += seg.GetLength();
                }
                _ASSERT(total_length == actual_length);
                _ASSERT(seg.GetPosition() == range_from);
                _ASSERT(seg.GetLength() == 0);
                total_length = 0;
                for ( ++seg; seg; ++seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    total_length += seg.GetLength();
                }
                _ASSERT(total_length == actual_length);
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

        if ( used_memory_check ) {
            if ( args["reset_scope"] ) {
                scope.ResetHistory();
                handle.Reset();
            }
            exit(0);
        }

        handle.Reset();
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.101  2005/03/14 18:12:44  vasilche
* Allow loading Seq-annot from file.
*
* Revision 1.100  2005/03/10 20:57:28  vasilche
* New way of CGBLoader instantiation.
*
* Revision 1.99  2005/02/24 17:13:20  vasilche
* Added flag -print_mapper to check CSeq_loc_Mapper.
* Added scan of all features in TSE when -whole_tse is specified.
* Avoid failure when cdregions don't have product.
*
* Revision 1.98  2005/02/01 21:55:11  grichenk
* Added direction flag for mapping between top level sequence
* and segments.
*
* Revision 1.97  2005/01/27 20:06:17  grichenk
* Fixed unused variable warning
*
* Revision 1.96  2005/01/05 18:58:41  vasilche
* Added -noexternal option.
*
* Revision 1.95  2004/12/23 17:01:45  grichenk
* Use BDB_CACHE_LIB and HAVE_BDB_CACHE
*
* Revision 1.94  2004/12/22 15:56:45  vasilche
* Added test for auto-release of TSEs by scope
*
* Revision 1.93  2004/12/21 18:57:35  vasilche
* Display second location on cdregion features.
*
* Revision 1.92  2004/12/06 17:54:09  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.91  2004/11/16 21:41:11  grichenk
* Removed default value for CScope* argument in CSeqMap methods
*
* Revision 1.90  2004/11/01 19:33:08  grichenk
* Removed deprecated methods
*
* Revision 1.89  2004/10/26 20:03:33  vasilche
* Added output of CSeq_loc_Mapper result.
*
* Revision 1.88  2004/10/21 15:48:40  vasilche
* Added options -range_loc and -overlap.
*
* Revision 1.87  2004/10/14 17:44:52  vasilche
* Added bidirectional tests for CSeqMap_CI.
*
* Revision 1.86  2004/10/07 14:09:48  vasilche
* More options to control demo application.
*
* Revision 1.85  2004/10/05 21:05:35  vasilche
* Added option -by_product.
*
* Revision 1.84  2004/09/27 14:37:24  grichenk
* More args (missing and external)
*
* Revision 1.83  2004/09/08 16:43:31  vasilche
* More tuning of BDB cache.
*
* Revision 1.82  2004/08/31 14:15:46  vasilche
* Added options -count_types and -reset_scope
*
* Revision 1.81  2004/08/24 16:43:53  vasilche
* Removed TAB symbols from sources.
* Seq-id cache is put in the same directory as blob cache.
* Removed dead code.
*
* Revision 1.80  2004/08/19 17:02:35  vasilche
* Use CBlob_id instead of obsolete CSeqref.
* Use requested feature subtype for all feature iterations.
* Added -limit_tse test option.
*
* Revision 1.79  2004/08/17 15:41:20  vasilche
* Added -used_memory_check option.
*
* Revision 1.78  2004/08/11 17:58:17  vasilche
* Added -print_cds option.
*
* Revision 1.77  2004/08/09 15:55:57  vasilche
* Fine tuning of blob cache.
*
* Revision 1.76  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.75  2004/07/21 15:51:24  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.74  2004/07/15 16:50:51  vasilche
* All annotation iterators use -range_from and -range_to options.
*
* Revision 1.73  2004/07/13 21:09:09  vasilche
* Added "range_from" and "range_to" options.
*
* Revision 1.72  2004/07/13 14:04:28  vasilche
* Optional compilation with Berkley DB.
*
* Revision 1.71  2004/07/12 19:21:14  ucko
* Don't assume that size_t == unsigned.
*
* Revision 1.70  2004/07/12 17:08:01  vasilche
* Allow limited processing to have benefits from split blobs.
*
* Revision 1.69  2004/06/30 22:15:20  vasilche
* Removed obsolete code for old blob cache interface.
*
* Revision 1.68  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.67  2004/05/13 19:50:48  vasilche
* Added options to skip and get mapped alignments.
*
* Revision 1.66  2004/05/13 19:33:15  vasilche
* Added option for printing descriptors.
*
* Revision 1.65  2004/04/27 19:00:08  kuznets
* Commented out old cache modes
*
* Revision 1.64  2004/04/09 20:37:48  vasilche
* Added optional test to get mapped location and object for graphs.
*
* Revision 1.63  2004/04/08 14:11:57  vasilche
* Added option to dump loaded Seq-entry.
*
* Revision 1.62  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.61  2004/03/18 16:30:24  grichenk
* Changed type of seq-align containers from list to vector.
*
* Revision 1.60  2004/02/09 19:18:50  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.59  2004/02/09 14:54:22  vasilche
* Dump synonyms of the sequence.
*
* Revision 1.58  2004/02/05 14:53:53  vasilche
* Used "new" cache interface by default.
*
* Revision 1.57  2004/02/04 18:05:34  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.56  2004/01/30 19:18:49  vasilche
* Added -seq_map flag.
*
* Revision 1.55  2004/01/29 20:38:47  vasilche
* Removed loading of whole sequence by CSeqMap_CI.
*
* Revision 1.54  2004/01/26 18:06:35  vasilche
* Add option for printing Seq-align.
* Use MSerial_Asn* manipulators.
* Removed unused includes.
*
* Revision 1.53  2004/01/07 17:38:02  vasilche
* Fixed include path to genbank loader.
*
* Revision 1.52  2004/01/05 18:14:03  vasilche
* Fixed name of project and path to header.
*
* Revision 1.51  2003/12/30 20:00:41  vasilche
* Added test for CGBDataLoader::GetSatSatKey().
*
* Revision 1.50  2003/12/30 19:51:54  vasilche
* Test CGBDataLoader::GetSatSatkey() method.
*
* Revision 1.49  2003/12/30 16:01:41  vasilche
* Added possibility to specify type of cache to use: -cache_mode (old|newid|new).
*
* Revision 1.48  2003/12/19 19:50:22  vasilche
* Removed obsolete split code.
* Do not use intemediate gi.
*
* Revision 1.47  2003/12/02 23:20:22  vasilche
* Allow to specify any Seq-id via ASN.1 text.
*
* Revision 1.46  2003/11/26 17:56:01  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.45  2003/10/27 15:06:31  vasilche
* Added option to set ID1 cache keep time.
*
* Revision 1.44  2003/10/21 14:27:35  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.43  2003/10/14 18:29:05  vasilche
* Added -exclude_named option.
*
* Revision 1.42  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.41  2003/10/08 17:55:32  vasilche
* Print sequence gi when -id option is used.
*
* Revision 1.40  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.39  2003/09/30 20:13:38  vasilche
* Print original feature location if requested.
*
* Revision 1.38  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.37  2003/08/27 14:22:01  vasilche
* Added options get_mapped_location, get_mapped_feature and get_original_feature
* to test feature iterator speed.
*
* Revision 1.36  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.35  2003/08/14 20:05:20  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.34  2003/07/25 21:41:32  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.33  2003/07/25 15:25:27  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.32  2003/07/24 20:36:17  vasilche
* Added arguemnt to choose ID1<->PUBSEQOS on Windows easier.
*
* Revision 1.31  2003/07/17 20:06:18  vasilche
* Added OBJMGR_LIBS definition.
*
* Revision 1.30  2003/07/17 19:10:30  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.29  2003/07/08 15:09:44  vasilche
* Added argument to test depth of segment resolution.
*
* Revision 1.28  2003/06/25 20:56:32  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.27  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.26  2003/05/27 20:54:52  grichenk
* Fixed CRef<> to bool convertion
*
* Revision 1.25  2003/05/06 16:52:54  vasilche
* Added 'pause' argument.
*
* Revision 1.24  2003/04/29 19:51:14  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.23  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.22  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.21  2003/04/03 14:17:43  vasilche
* Allow using data loaded from file.
*
* Revision 1.20  2003/03/27 19:40:11  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
* Revision 1.19  2003/03/26 17:27:04  vasilche
* Added optinal reverse feature traversal.
*
* Revision 1.18  2003/03/21 14:51:41  vasilche
* Added debug printing of features collected.
*
* Revision 1.17  2003/03/18 21:48:31  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.16  2003/03/10 16:33:44  vasilche
* Added dump of features if errors were detected.
*
* Revision 1.15  2003/02/27 20:57:36  vasilche
* Addef some options for better performance testing.
*
* Revision 1.14  2003/02/24 19:02:01  vasilche
* Reverted testing shortcut.
*
* Revision 1.13  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.12  2002/12/06 15:36:01  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.11  2002/12/05 19:28:33  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.10  2002/11/08 19:43:36  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.9  2002/11/04 21:29:13  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.8  2002/10/02 17:58:41  grichenk
* Added CBioseq_CI sample code
*
* Revision 1.7  2002/09/03 21:27:03  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.6  2002/06/12 14:39:03  grichenk
* Renamed enumerators
*
* Revision 1.5  2002/05/06 03:28:49  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/05/03 18:37:34  grichenk
* Added more examples of using CFeat_CI and GetSequenceView()
*
* Revision 1.2  2002/03/28 14:32:58  grichenk
* Minor fixes
*
* Revision 1.1  2002/03/28 14:07:25  grichenk
* Initial revision
*
*
* ===========================================================================
*/
