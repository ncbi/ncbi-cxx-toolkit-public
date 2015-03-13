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
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqres/seqres__.hpp>
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
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
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
#include <objtools/data_loaders/genbank/seqref.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#ifdef HAVE_LIBSQLITE3
#  define HAVE_LDS2 1
#elif defined(HAVE_LDS2)
#  undef HAVE_LDS2
#endif

#ifdef HAVE_LDS2
#  include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#  include <objtools/lds2/lds2.hpp>
#endif

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

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
    virtual void Exit(void);
    void GetIds(CScope& scope, const CSeq_id_Handle& idh);
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
    arg_desc->AddOptionalKey("blob_id", "BlobId",
                             "sat/satkey of Genbank entry to load",
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
    arg_desc->AddOptionalKey("bioseq_file", "SeqAnnotFile",
                             "file with Bioseq to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("bioseq_bfile", "SeqAnnotFile",
                             "file with Bioseq to load (binary ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("count", "RepeatCount",
                            "repeat test work RepeatCount times",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("pause", "Pause",
                            "pause between tests in seconds",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddFlag("pause_key", "pause and wait for ENTER between tests");

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
#ifdef HAVE_LDS2
    arg_desc->AddOptionalKey("lds_dir", "LDSDir",
                             "Use local data storage loader from the specified firectory",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("lds_db", "LDSDB",
                             "Use local data storage loader from the specified LDS2 DB",
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
    arg_desc->AddOptionalKey("csra", "cSRA",
                             "Add cSRA accessions (comma separated)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("other_loaders", "OtherLoaders",
                             "Extra data loaders as plugins (comma separated)",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("get_ids", "Get sequence ids");
    arg_desc->AddFlag("get_blob_id", "Get sequence blob id");
    arg_desc->AddFlag("get_gi", "Get sequence gi");
    arg_desc->AddFlag("get_acc", "Get sequence accession");
    arg_desc->AddFlag("get_taxid", "Get TaxId");
    arg_desc->AddFlag("get_title", "Get sequence title");

    arg_desc->AddFlag("seq_map", "scan SeqMap on full depth");
    arg_desc->AddFlag("seg_labels", "get labels of all segments in Delta");
    arg_desc->AddFlag("whole_sequence", "load whole sequence");
    arg_desc->AddFlag("scan_whole_sequence", "scan whole sequence");
    arg_desc->AddFlag("scan_whole_sequence2", "scan whole sequence w/o iterator");
    arg_desc->AddFlag("check_gaps", "check sequence gaps during scanning");
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
    arg_desc->AddFlag("get_types",
                      "print only types of features found");
    arg_desc->AddFlag("get_names",
                      "print only Seq-annot names of features found");
    arg_desc->AddOptionalKey("range_from", "RangeFrom",
                             "features starting at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("range_to", "RangeTo",
                             "features ending at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("range_step", "RangeStep",
                             "shift range by this value between iterations",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("plus_strand",
                      "use plus strand of the sequence");
    arg_desc->AddFlag("minus_strand",
                      "use minus strand of the sequence");
    arg_desc->AddFlag("ignore_strand",
                      "ignore strand of feature location");
    arg_desc->AddOptionalKey("range_loc", "RangeLoc",
                             "features on this Seq-loc in ASN.1 text format",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("overlap", "Overlap",
                            "Method of overlap location check",
                            CArgDescriptions::eString, "totalrange");
    arg_desc->SetConstraint("overlap",
                            &(*new CArgAllow_Strings,
                              "totalrange", "intervals"));
    arg_desc->AddFlag("no_map", "Do not map features to master sequence");
                             
    arg_desc->AddFlag("get_mapped_location", "get mapped location");
    arg_desc->AddFlag("get_original_feature", "get original location");
    arg_desc->AddFlag("get_mapped_feature", "get mapped feature");
    arg_desc->AddFlag("get_feat_handle", "reverse lookup of feature handle");
    arg_desc->AddFlag("sort_seq_feat", "sort CSeq_feat objects");
    arg_desc->AddFlag("save_mapped_feat", "save and check CMappedFeat objects");
    arg_desc->AddFlag("check_cds", "check correctness cds");
    arg_desc->AddFlag("check_seq_data", "check availability of seq_data");
    arg_desc->AddFlag("seq_vector_tse", "use TSE as a base for CSeqVector");
    arg_desc->AddFlag("skip_graphs", "do not search for graphs");
    arg_desc->AddFlag("skip_alignments", "do not search for alignments");
    arg_desc->AddFlag("print_alignments", "print all found Seq-aligns");
    arg_desc->AddFlag("get_mapped_alignments", "get mapped alignments");
    arg_desc->AddFlag("reverse", "reverse order of features");
    arg_desc->AddFlag("labels", "compare features by labels too");
    arg_desc->AddFlag("no_sort", "do not sort features");
    arg_desc->AddDefaultKey("max_feat", "MaxFeat",
                            "Max number of features to iterate",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddOptionalKey("max_search_segments", "MaxSearchSegments",
                            "Max number of empty segments to search",
                            CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("max_search_time", "MaxSearchTime",
                            "Max time to search for a first annotation",
                            CArgDescriptions::eDouble);
    arg_desc->AddDefaultKey("depth", "depth",
                            "Max depth of segments to iterate",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddFlag("adaptive", "Use adaptive depth of segments");
    arg_desc->AddFlag("no-feat-policy", "Ignore feature fetch policy");
    arg_desc->AddFlag("only-feat-policy", "Adaptive by feature fetch policy only");
    arg_desc->AddFlag("exact_depth", "Use exact depth of segments");
    arg_desc->AddFlag("unnamed",
                      "include features from unnamed Seq-annots");
    arg_desc->AddOptionalKey("named", "NamedAnnots",
                             "include features from named Seq-annots "
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("named_acc", "NamedAnnotAccession",
                             "include features with named annot accession "
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
    arg_desc->AddOptionalKey("feat_id_str", "FeatIdStr",
                             "String Feat-id of features to search",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("make_tree", "make feature tree");
    arg_desc->AddDefaultKey("feat_id_mode", "feat_id_mode",
                            "CFeatTree xref by feat id mode",
                            CArgDescriptions::eString,
                            "by_type");
    arg_desc->SetConstraint("feat_id_mode",
                            &(*new CArgAllow_Strings,
                              "ignore", "by_type", "always"));
    arg_desc->AddDefaultKey("snp_strand_mode", "snp_strand_mode",
                            "CFeatTree SNP strand mode",
                            CArgDescriptions::eString,
                            "both");
    arg_desc->SetConstraint("snp_strand_mode",
                            &(*new CArgAllow_Strings,
                              "same", "both"));

    arg_desc->AddFlag("print_tree", "print feature tree");
    arg_desc->AddFlag("dump_seq_id", "dump CSeq_id_Handle usage");
    arg_desc->AddFlag("used_memory_check", "exit(0) after loading sequence");
    arg_desc->AddFlag("reset_scope", "reset scope before exiting");
    arg_desc->AddFlag("modify", "try to modify Bioseq object");
    arg_desc->AddOptionalKey("table_field_name", "table_field_name",
                             "Table Seq-feat field name to retrieve",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("table_field_id", "table_field_id",
                             "Table Seq-feat field id to retrieve",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("print_seq_table", "print all found Seq-tables");

    arg_desc->AddOptionalKey("save_NA", "save_NA_prefix",
                             "Save named annotations blobs",
                             CArgDescriptions::eString);
    
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


class CPrefetchSeqData
    : public CPrefetchBioseq
{
public:
    typedef string TResult;

    // from bioseq
    CPrefetchSeqData(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     CSeq_data::E_Choice encoding)
        : CPrefetchBioseq(bioseq),
          m_Range(range),
          m_Strand(strand),
          m_Encoding(encoding),
          m_VectorCoding(CBioseq_Handle::eCoding_NotSet)
        {
        }
    CPrefetchSeqData(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     CBioseq_Handle::EVectorCoding vector_coding)
        : CPrefetchBioseq(bioseq),
          m_Range(range),
          m_Strand(strand),
          m_Encoding(CSeq_data::e_not_set),
          m_VectorCoding(vector_coding)
        {
        }

    virtual bool Execute(CRef<CPrefetchRequest> token)
        {
            if ( !CPrefetchBioseq::Execute(token) ) {
                return false;
            }
            CSeqVector sv(GetBioseqHandle(), m_VectorCoding, m_Strand);
            if ( m_Encoding != CSeq_data::e_not_set ) {
                sv.SetCoding(m_Encoding);
            }
            sv.GetSeqData(m_Range.GetFrom(), m_Range.GetToOpen(), m_Result);
            return true;
        }

    const string& GetSequence(void) const
        {
            return m_Result;
        }
    const string& GetResult(void) const
        {
            return m_Result;
        }

private:
    // from bioseq
    CRange<TSeqPos>     m_Range;
    ENa_strand          m_Strand;
    // encoding
    CSeq_data::E_Choice m_Encoding;
    CBioseq_Handle::EVectorCoding m_VectorCoding;
    // result
    TResult             m_Result;
};


CSeq_id_Handle s_Normalize(const CSeq_id_Handle& id, CScope& scope)
{
    CSeq_id_Handle ret = scope.GetAccVer(id);
    return ret? ret: id;
}


typedef pair<string, CMappedFeat> TFeatureKey;
typedef set<TFeatureKey> TOrderedFeatures;
typedef map<CMappedFeat, TOrderedFeatures> TOrderedTree;
typedef map<TFeatureKey, size_t> TFeatureIndex;

TFeatureKey s_GetFeatureKey(const CMappedFeat& child)
{
    CNcbiOstrstream str;
    CRange<TSeqPos> range;
    try {
        range = child.GetLocation().GetTotalRange();
    }
    catch ( CException& ) {
    }
    str << setw(10) << range.GetFrom()
        << setw(10) << range.GetTo()
        << " " << MSerial_AsnText
        << child.GetMappedFeature();
    string s = CNcbiOstrstreamToString(str);
    return TFeatureKey(s, child);
}

ostream& operator<<(ostream& out, const CMappedFeat& feat)
{
    out << CSeqFeatData::SelectionName(feat.GetFeatType())
        << "(subt " << feat.GetFeatSubtype() << ")";
    if ( feat.GetFeatType() == CSeqFeatData::e_Gene ) {
        const CGene_ref& gene = feat.GetOriginalFeature().GetData().GetGene();
        if ( gene.IsSetLocus() ) {
            out << " " << gene.GetLocus();
        }
        if ( gene.IsSetLocus_tag() ) {
            out << " tag=" << gene.GetLocus_tag();
        }
    }
    if ( feat.IsSetProduct() ) {
        out << " -> ";
        CConstRef<CSeq_id> id;
        try {
            id = feat.GetProduct().GetId();
        }
        catch ( CException& ) {
            out << "*bad loc*";
        }
        if ( id ) {
            out << s_Normalize(CSeq_id_Handle::GetHandle(*id),
                               feat.GetScope());
        }
    }
    out << " ";
    try {
        out << feat.GetLocation().GetTotalRange();
    }
    catch ( CException& ) {
        out << "*bad loc*";
    }
    return out;
}

void s_PrintTree(const string& p1, const string& p2,
                 TOrderedTree& tree, TFeatureKey key,
                 TFeatureIndex& index)
{
    const CMappedFeat& feat = key.second;
    const TOrderedFeatures& cc = tree[feat];
    NcbiCout << p1 << "-F[" << index[key] << "]: " << feat << "\n";
    ITERATE ( TOrderedFeatures, it, cc ) {
        TOrderedFeatures::const_iterator it2 = it;
        if ( ++it2 != cc.end() ) {
            s_PrintTree(p2+" +", p2+" |", tree, *it, index);
        }
        else {
            s_PrintTree(p2+" +", p2+"  ", tree, *it, index);
        }
    }
}

void CDemoApp::GetIds(CScope& scope, const CSeq_id_Handle& idh)
{
    const CArgs& args = GetArgs();

    if ( args["get_gi"] ) {
        NcbiCout << "Gi: "
                 << sequence::GetId(idh, scope, sequence::eGetId_ForceGi)
                 << NcbiEndl;
    }
    if ( args["get_acc"] ) {
        if ( args["gi"] ) {
            TGi gi = GI_FROM(int, args["gi"].AsInteger());
            NcbiCout << "Acc: "
                     << sequence::GetAccessionForGi(gi, scope, sequence::eWithoutAccessionVersion)
                     << NcbiEndl;
        }
    }
    if ( args["get_taxid"] ) {
        NcbiCout << "TaxId: "
                 << scope.GetTaxId(idh)
                 << NcbiEndl;
    }
    CSeq_id_Handle best_id =
        sequence::GetId(idh, scope, sequence::eGetId_Best);
    if ( best_id ) {
        NcbiCout << "Best id: " << best_id << NcbiEndl;
    }
    else {
        NcbiCout << "Best id: null" << NcbiEndl;
    }
    NcbiCout << "Ids:" << NcbiEndl;
    //scope.GetBioseqHandle(idh);
    vector<CSeq_id_Handle> ids = scope.GetIds(idh);
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        string l;
        it->GetSeqId()->GetLabel(&l, CSeq_id::eContent, CSeq_id::fLabel_Version);
        NcbiCout << "    " << it->AsString() << " : " << l << NcbiEndl;
    }
}


void x_Pause(const char* msg, bool pause_key)
{
    if ( pause_key ) {
        NcbiCout << "Press enter before "<< msg << NcbiFlush;
        string s;
        getline(NcbiCin, s);
    }
}


int CDemoApp::Run(void)
{
    SetDiagPostLevel(eDiag_Info);

    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id;
    CRef<CSeq_loc> range_loc;
    if ( args["gi"] ) {
        TGi gi = GI_FROM(int, args["gi"].AsInteger());
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
        CObjectIStreamAsn in(text.data(), text.size());
        string h = in.ReadFileHeader();
        if ( h == "Seq-id" ) {
            in.ReadObject(&*id, CSeq_id::GetTypeInfo());
        }
        else if ( h == "Seq-loc" ) {
            range_loc = new CSeq_loc;
            in.ReadObject(&*range_loc, CSeq_loc::GetTypeInfo());
            id = SerialClone(*range_loc->GetId());
        }
        else {
            ERR_FATAL("Unknown asn_id type: "<<args["asn_id"].AsString());
        }
    }
    else {
        ERR_FATAL("One of -gi, -id or -asn_id arguments is required");
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
    bool pause_key = args["pause_key"];
    bool only_features = args["only_features"];
    bool by_product = args["by_product"];
    bool count_types = args["count_types"];
    bool count_subtypes = args["count_subtypes"];
    bool get_types = args["get_types"];
    bool get_names = args["get_names"];
    if ( get_types || get_names ) {
        only_features = true;
    }
    if ( count_types || count_subtypes ) {
        only_features = true;
    }
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
    bool get_feat_handle = args["get_feat_handle"];
    bool print_alignments = args["print_alignments"];
    bool check_cds = args["check_cds"];
    bool check_seq_data = args["check_seq_data"];
    bool seq_vector_tse = args["seq_vector_tse"];
    bool skip_graphs = args["skip_graphs"];
    bool skip_alignments = args["skip_alignments"];
    bool get_mapped_alignments = args["get_mapped_alignments"];
    SAnnotSelector::ESortOrder order =
        args["reverse"] ?
        SAnnotSelector::eSortOrder_Reverse : SAnnotSelector::eSortOrder_Normal;
    if ( args["no_sort"] )
        order = SAnnotSelector::eSortOrder_None;
    bool sort_seq_feat = args["sort_seq_feat"];
    bool save_mapped_feat = args["save_mapped_feat"];
    bool labels = args["labels"];
    int max_feat = args["max_feat"].AsInteger();
    int depth = args["depth"].AsInteger();
    bool adaptive = args["adaptive"];
    bool no_feat_policy = args["no-feat-policy"];
    bool only_feat_policy = args["only-feat-policy"];
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
    bool scan_whole_sequence = args["scan_whole_sequence"];
    bool scan_whole_sequence2 = args["scan_whole_sequence2"];
    bool check_gaps = args["check_gaps"];
    bool dump_seq_id = args["dump_seq_id"];
    bool used_memory_check = args["used_memory_check"];
    bool get_synonyms = true;
    bool get_ids = args["get_ids"];
    bool get_blob_id = args["get_blob_id"];
    bool make_tree = args["make_tree"];
    feature::CFeatTree::EFeatIdMode feat_id_mode =
        feature::CFeatTree::eFeatId_by_type;
    if ( args["feat_id_mode"].AsString() == "ignore" ) {
        feat_id_mode = feature::CFeatTree::eFeatId_ignore;
    }
    else if ( args["feat_id_mode"].AsString() == "always" ) {
        feat_id_mode = feature::CFeatTree::eFeatId_always;
    }
    feature::CFeatTree::ESNPStrandMode snp_strand_mode =
        feature::CFeatTree::eSNPStrand_both;
    if ( args["snp_strand_mode"].AsString() == "same" ) {
        snp_strand_mode = feature::CFeatTree::eSNPStrand_same;
    }
    else if ( args["snp_strand_mode"].AsString() == "both" ) {
        snp_strand_mode = feature::CFeatTree::eSNPStrand_both;
    }
    bool print_tree = args["print_tree"];
    list<string> include_named;
    if ( args["named"] ) {
        NStr::Split(args["named"].AsString(), ",", include_named);
    }
    list<string> exclude_named;
    if ( args["exclude_named"] ) {
        NStr::Split(args["exclude_named"].AsString(), ",", exclude_named);
    }
    list<string> include_named_accs;
    if ( args["named_acc"] ) {
        NStr::Split(args["named_acc"].AsString(), ",", include_named_accs);
    }
    string save_NA_prefix = args["save_NA"]? args["save_NA"].AsString(): "";
    bool scan_seq_map = args["seq_map"];
    bool get_seg_labels = args["seg_labels"];

    vector<int> types_counts, subtypes_counts;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    CRef<CGBDataLoader> gb_loader;
    vector<string> other_loaders;
    if ( args["loader"] ) {
        string genbank_readers = args["loader"].AsString();
        if ( genbank_readers != "-" ) {
            // Create genbank data loader and register it with the OM.
            // The last argument "eDefault" informs the OM that the loader
            // must be included in scopes during the CScope::AddDefaults() call
#ifdef HAVE_PUBSEQ_OS
            DBAPI_RegisterDriver_FTDS();
            GenBankReaders_Register_Pubseq();
#endif
            gb_loader = CGBDataLoader::RegisterInObjectManager
                (*pOm, genbank_readers).GetLoader();
        }
    }
    else {
#ifdef HAVE_PUBSEQ_OS
        DBAPI_RegisterDriver_FTDS();
        GenBankReaders_Register_Pubseq();
#endif
        gb_loader = CGBDataLoader::RegisterInObjectManager(*pOm).GetLoader();
    }
#ifdef HAVE_LDS2
    if ( args["lds_dir"] || args["lds_db"] ) {
        string lds_db, lds_dir;
        if ( args["lds_db"] ) {
            lds_db = args["lds_db"].AsString();
            if ( args["lds_dir"] ) {
                lds_dir = args["lds_dir"].AsString();
            }
        }
        else {
            lds_dir = args["lds_dir"].AsString();
            lds_db = CDirEntry::ConcatPath(lds_dir, "lds2.db");
        }
        if ( !CDirEntry(lds_db).Exists() && !lds_dir.empty() ) {
            CLDS2_Manager manager(lds_db);
            manager.AddDataDir(lds_dir, CLDS2_Manager::eDir_Recurse);
            manager.UpdateData();
        }
        other_loaders.push_back(CLDS2_DataLoader::RegisterInObjectManager(*pOm, lds_db).GetLoader()->GetName());
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
        other_loaders.push_back(CBlastDbDataLoader::RegisterInObjectManager(*pOm, db, type).GetLoader()->GetName());
    }
    if ( args["csra"] ) {
        string old_param = GetConfig().Get("CSRA", "ACCESSIONS");
        GetConfig().Set("CSRA", "ACCESSIONS", args["csra"].AsString());
        other_loaders.push_back(pOm->RegisterDataLoader(0, "csra")->GetName());
        GetConfig().Set("CSRA", "ACCESSIONS", old_param);
    }
    if ( args["other_loaders"] ) {
        list<string> names;
        NStr::Split(args["other_loaders"].AsString(), ",", names);
        ITERATE ( list<string>, i, names ) {
            other_loaders.push_back(pOm->RegisterDataLoader(0, *i)->GetName());
        }
    }

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();
    ITERATE ( vector<string>, it, other_loaders ) {
        scope.AddDataLoader(*it, 88);
    }

    CSeq_entry_Handle added_entry;
    CSeq_annot_Handle added_annot;
    CBioseq_Handle added_seq;
    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["file"].AsInputFile() >> MSerial_AsnText >> *entry;
        if ( used_memory_check ) {
            exit(0);
        }
        added_entry = scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*entry));
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
        NcbiCout << "Added annot file: "<<args["annot_file"]<<NcbiEndl;
    }
    if ( args["annot_bfile"] ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        args["annot_bfile"].AsInputFile() >> MSerial_AsnBinary >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }
    if ( args["bioseq_file"] ) {
        CRef<CBioseq> seq(new CBioseq);
        args["bioseq_file"].AsInputFile() >> MSerial_AsnText >> *seq;
        added_seq = scope.AddBioseq(*seq);
        NcbiCout << "Added bioseq file: "<<args["bioseq_file"]<<NcbiEndl;
    }
    if ( args["bioseq_bfile"] ) {
        CRef<CBioseq> seq(new CBioseq);
        args["bioseq_bfile"].AsInputFile() >> MSerial_AsnBinary >> *seq;
        added_seq = scope.AddBioseq(*seq);
    }
    if ( args["blob_id"] ) {
        string str = args["blob_id"].AsString();
        vector<string> keys;
        NStr::Tokenize(str, "/", keys);
        if ( keys.size() < 2 || keys.size() > 3 ) {
            ERR_FATAL("Bad blob_id: "<<str<<". Should be sat/satkey(/subsat)?");
        }
        if ( !gb_loader ) {
            ERR_FATAL("Cannot load by blob_id without Genbank loader");
        }
        int sat, satkey, subsat = 0;
        sat = NStr::StringToInt(keys[0]);
        satkey = NStr::StringToInt(keys[1]);
        if ( keys.size() == 3 ) {
            subsat = NStr::StringToInt(keys[2]);
        }
        CScope::TBlobId blob_id =
            gb_loader->GetBlobIdFromSatSatKey(sat, satkey, subsat);
        CSeq_entry_Handle seh = scope.GetSeq_entryHandle(gb_loader, blob_id);
        if ( !seh ) {
            ERR_FATAL("Genbank entry with blob_id "<<str<<" not found");
        }
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    if ( get_ids ) {
        GetIds(scope, idh);
    }
    if ( get_blob_id && gb_loader && other_loaders.empty() ) {
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
    if ( handle && args["get_title"] ) {
        NcbiCout << "Title: \"" << sequence::CDeflineGenerator().GenerateDefline(handle) << "\""
                 << NcbiEndl;
    }
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Error << "Bioseq not found.");
    }
    if ( handle && get_synonyms ) {
        NcbiCout << "Synonyms:" << NcbiEndl;
        CConstRef<CSynonymsSet> syns = scope.GetSynonyms(handle);
        ITERATE ( CSynonymsSet, it, *syns ) {
            CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(it);
            NcbiCout << "    " << idh.AsString() << NcbiEndl;
        }
    }

    if ( handle && print_tse ) {
        CConstRef<CSeq_entry> entry =
            handle.GetTopLevelEntry().GetCompleteSeq_entry();
        NcbiCout << "-------------------- TSE --------------------\n";
        NcbiCout << MSerial_AsnText << *entry << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }
    if ( handle && print_seq ) {
        NcbiCout << "-------------------- SEQ --------------------\n";
        NcbiCout << MSerial_AsnText << *handle.GetCompleteObject() << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }

    CRef<CSeq_id> search_id = id;
    CRef<CSeq_loc> whole_loc(new CSeq_loc);
    // No region restrictions -- the whole bioseq is used:
    whole_loc->SetWhole(*search_id);
    bool plus_strand = args["plus_strand"];
    bool minus_strand = args["minus_strand"];
    bool ignore_strand = args["ignore_strand"];
    TSeqPos range_from, range_to;
    CRange<TSeqPos> range;
    ENa_strand strand;
    if ( plus_strand || minus_strand || args["range_from"] || args["range_to"] ) {
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
            range_to = handle? handle.GetBioseqLength()-1: kInvalidSeqPos;
        }
        range_loc.Reset(new CSeq_loc);
        range_loc->SetInt().SetId(*search_id);
        range_loc->SetInt().SetFrom(range_from);
        range_loc->SetInt().SetTo(range_to);
        range.SetFrom(range_from).SetTo(range_to);
        strand = eNa_strand_unknown;
        if ( plus_strand ) {
            range_loc->SetInt().SetStrand(strand = eNa_strand_plus);
        }
        else if ( minus_strand ) {
            range_loc->SetInt().SetStrand(strand = eNa_strand_minus);
        }
    }
    else if ( range_loc ) {
        range = range_loc->GetTotalRange();
        range_from = range.GetFrom();
        range_to = range.GetTo();
        strand = range_loc->GetStrand();
    }
    else {
        range_from = range_to = 0;
        range_loc = whole_loc;
        range = range.GetWhole();
        strand = eNa_strand_unknown;
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
    bool no_map = args["no_map"];

    string table_field_name;
    if ( args["table_field_name"] )
        table_field_name = args["table_field_name"].AsString();
    int table_field_id = -1;
    if ( args["table_field_id"] )
        table_field_id = args["table_field_id"].AsInteger();
    bool modify = args["modify"];

    handle.Reset();
    
    CRef<CPrefetchManager> prefetch_manager;
    //prefetch_manager = new CPrefetchManager;
    vector<CRef<CPrefetchRequest> > prefetch_snp;
    vector<CRef<CPrefetchRequest> > prefetch_seq;
    set<CSeq_feat_Handle> ff;
    
    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c ) {
            if ( get_ids ) {
                GetIds(scope, idh);
            }
        }
        if ( 1+c && pause ) {
            SleepSec(pause);
        }
        if ( c ) {
            NcbiCout << "Iteration " << c << NcbiEndl;
            if ( args["range_step"] && range_loc != whole_loc ) {
                TSeqPos step = args["range_step"].AsInteger();
                range_from += step;
                range_to += step;
                range_loc->SetInt().SetFrom(range_from);
                range_loc->SetInt().SetTo(range_to);
            }
        }

        // get handle again, check for scope TSE locking
        handle = scope.GetBioseqHandle(idh);
        if ( !handle ) {
            ERR_POST(Error << "Cannot resolve "<<idh.AsString());
            //continue;
        }

        if ( handle && get_seg_labels ) {
            x_Pause("getting seq map labels", pause_key);
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
        if ( handle && !only_features ) {
            // List other sequences in the same TSE
            if ( whole_tse ) {
                NcbiCout << "TSE sequences:" << NcbiEndl;
                for ( CBioseq_CI bit(handle.GetTopLevelEntry()); bit; ++bit) {
                    NcbiCout << "    "<<bit->GetSeqId()->DumpAsFasta()<<
                        NcbiEndl;
                }
            }

            // Get the bioseq
            CConstRef<CBioseq> bioseq(handle.GetBioseqCore());
            // -- use the bioseq: print the first seq-id
            NcbiCout << "First ID = " <<
                (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

            x_Pause("getting seq data", pause_key);
            // Get the sequence using CSeqVector. Use default encoding:
            // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
            CSeqVector seq_vect(*range_loc, scope,
                                CBioseq_Handle::eCoding_Iupac);
            if ( seq_vector_tse ) {
                seq_vect = CSeqVector(*range_loc, handle.GetTSE_Handle(),
                                      CBioseq_Handle::eCoding_Iupac);
            }
            //handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            // -- use the vector: print length and the first 10 symbols
            NcbiCout << "Sequence: length=" << seq_vect.size() << NcbiFlush;
            if ( check_seq_data ) {
                CStopWatch sw(CStopWatch::eStart);
                if ( seq_vect.CanGetRange(0, seq_vect.size()) ) {
                    NcbiCout << " data=";
                    sout.erase();
                    TSeqPos size = min(seq_vect.size(), 100u);
                    for ( TSeqPos i=0; i < size; ++i ) {
                        // Convert sequence symbols to printable form
                        sout += seq_vect[i];
                    }
                    NcbiCout << NStr::PrintableString(sout)
                             << " in " << sw;
                }
                else {
                    NcbiCout << " data unavailable"
                             << " in " << sw;
                }
            }
            else {
                try {
                    seq_vect[seq_vect.size()-1];
                    NcbiCout << " got last byte";
                }
                catch ( CException& exc ) {
                    ERR_POST(" cannot get last byte: Exception: "<<exc.what());
                }
            }
            NcbiCout << NcbiEndl;
            if ( whole_sequence ) {
                CStopWatch sw(CStopWatch::eStart);
                TSeqPos size = seq_vect.size();
                try {
                    NcbiCout << "Whole seq data["<<size<<"] = " << NcbiFlush;
                    seq_vect.GetSeqData(0, size, sout);
                    if ( size <= 20u ) {
                        NcbiCout << NStr::PrintableString(sout);
                    }
                    else {
                        NcbiCout << NStr::PrintableString(sout.substr(0, 10));
                        NcbiCout << "..";
                        NcbiCout << NStr::PrintableString(sout.substr(size-10));
                    }
                }
                catch ( CException& exc ) {
                    ERR_POST("GetSeqData() failed: "<<exc);
                }
                NcbiCout << " in " << sw << NcbiEndl;
            }
            if ( scan_whole_sequence ) {
                CStopWatch sw(CStopWatch::eStart);
                NcbiCout << "Scanning sequence..." << NcbiFlush;
                TSeqPos pos = 0;
                try {
                    string buffer;
                    for ( CSeqVector_CI it(seq_vect); it; ) {
                        _ASSERT(it.GetPos() == pos);
                        if ( check_gaps && it.IsInGap() ) {
                            NcbiCout << "Gap " << it.GetGapSizeForward()
                                     << " at "<<it.GetPos()<<": ";
                            CConstRef<CSeq_literal> gap =
                                it.GetGapSeq_literal();
                            if ( gap ) {
                                NcbiCout << MSerial_AsnText << *gap;
                            }
                            else {
                                NcbiCout << "unspecified" << NcbiEndl;
                            }
                            pos += it.GetGapSizeForward();
                            it.SkipGap();
                            continue;
                        }
                        if ( (pos & 0xffff) == 0 ) {
                            TSeqPos cnt = min(TSeqPos(99), seq_vect.size()-pos);
                            it.GetSeqData(buffer, cnt);
                            pos += cnt;
                        }
                        else {
                            ++it;
                            ++pos;
                        }
                        _ASSERT(it.GetPos() == pos);
                    }
                    _ASSERT(pos == seq_vect.size());
                }
                catch ( CException& exc ) {
                    ERR_POST("sequence scan failed at "<<pos<<": "<<exc);
                }
                NcbiCout << "done" << " in " << sw << NcbiEndl;
                _ASSERT(!seq_vect.IsInGap(1));
            }
            if ( scan_whole_sequence2 ) {
                CStopWatch sw(CStopWatch::eStart);
                NcbiCout << "Scanning sequence..." << NcbiFlush;
                TSeqPos pos = 0;
                try {
                    string buffer;
                    CSeqVector::TMutexGuard guard(seq_vect.GetMutex());
                    for ( ; pos < seq_vect.size(); ++pos ) {
                        if ( check_gaps && seq_vect.IsInGap(pos) ) {
                            TSeqPos gap_size = seq_vect.GetGapSizeForward(pos);
                            NcbiCout << "Gap " << gap_size
                                     << " at "<<pos<<": ";
                            CConstRef<CSeq_literal> gap =
                                seq_vect.GetGapSeq_literal(pos);
                            if ( gap ) {
                                NcbiCout << MSerial_AsnText << *gap;
                            }
                            else {
                                NcbiCout << "unspecified" << NcbiEndl;
                            }
                            pos += gap_size;
                            continue;
                        }
                        if ( (pos & 0xffff) == 0 ) {
                            TSeqPos cnt = min(TSeqPos(99), seq_vect.size()-pos);
                            seq_vect.GetSeqData(pos, pos+cnt, buffer);
                            pos += cnt;
                        }
                        else {
                            seq_vect[pos];
                            ++pos;
                        }
                    }
                    _ASSERT(pos == seq_vect.size());
                }
                catch ( CException& exc ) {
                    ERR_POST("sequence scan failed at "<<pos<<": "<<exc);
                }
                NcbiCout << "done" << " in " << sw << NcbiEndl;
            }
            // CSeq_descr iterator: iterates all descriptors starting
            // from the bioseq and going the seq-entries tree up to the
            // top-level seq-entry.
            count = 0;
            x_Pause("getting seq desc", pause_key);
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
            .SetNoMapping(no_map)
            .SetSortOrder(order)
            .SetMaxSize(max_feat)
            .SetResolveDepth(depth)
            .SetAdaptiveDepth(adaptive)
            .SetExactDepth(exact_depth)
            .SetUnresolvedFlag(missing)
            .SetIgnoreStrand(ignore_strand);
        if ( args["max_search_segments"] ) {
            base_sel.SetMaxSearchSegments(args["max_search_segments"].AsInteger());
        }
        if ( args["max_search_time"] ) {
            base_sel.SetMaxSearchTime(float(args["max_search_time"].AsDouble()));
        }
        if ( no_feat_policy ) {
            base_sel.SetAdaptiveDepthFlags(base_sel.GetAdaptiveDepthFlags()&
                                           ~SAnnotSelector::fAdaptive_ByPolicy);
        }
        if ( only_feat_policy ) {
            base_sel.SetAdaptiveDepthFlags(SAnnotSelector::fAdaptive_ByPolicy);
        }
        if ( labels ) {
            base_sel.SetFeatComparator(new feature::CFeatComparatorByLabel());
        }
        if ( handle && externals_only ) {
            base_sel.SetSearchExternal(handle);
        }
        if ( limit_tse ) {
            if ( added_annot ) {
                base_sel.SetLimitSeqAnnot(added_annot);
            }
            else if ( added_entry ) {
                base_sel.SetLimitSeqEntry(added_entry);
            }
            else if ( handle ) {
                base_sel.SetLimitTSE(handle.GetTopLevelEntry());
            }
        }
        if ( include_allnamed ) {
            base_sel.SetAllNamedAnnots();
        }
        if ( include_unnamed ) {
            base_sel.AddUnnamedAnnots();
        }
        ITERATE ( list<string>, it, include_named ) {
            base_sel.AddNamedAnnots(*it);
        }
        ITERATE ( list<string>, it, include_named_accs ) {
            base_sel.IncludeNamedAnnotAccession(*it);
        }
        if ( nosnp ) {
            base_sel.ExcludeNamedAnnots("SNP");
        }
        ITERATE ( list<string>, it, exclude_named ) {
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

        CStopWatch sw;

        if ( prefetch_manager ) {
            // Initialize prefetch token;
            SAnnotSelector snp_sel = base_sel;
            snp_sel.ResetAnnotsNames();
            snp_sel.AddNamedAnnots("SNP");
            prefetch_seq.clear();
            prefetch_snp.clear();
            TSeqPos step = args["range_step"].AsInteger();
            for ( int i = 0; i < 2; ++i ) {
                ENa_strand strand =
                    minus_strand? eNa_strand_minus: eNa_strand_plus;
                TSeqPos from = range_from + step/2*i;
                TSeqPos to = range_to + step/2*i;
                prefetch_snp.push_back
                    (CStdPrefetch::GetFeat_CI(*prefetch_manager,
                                              handle,
                                              CRange<TSeqPos>(from, to),
                                              strand,
                                              snp_sel));
                prefetch_seq.push_back
                    (prefetch_manager->AddAction
                     (new CPrefetchSeqData(handle,
                                           CRange<TSeqPos>(from, to),
                                           strand,
                                           CBioseq_Handle::eCoding_Iupac)));
            }
        }
        
        if ( get_types || get_names ) {
            if ( get_types ) {
                sw.Restart();
                CFeat_CI it(scope, *range_loc, base_sel.SetCollectTypes());
                ITERATE ( CFeat_CI::TAnnotTypes, i, it.GetAnnotTypes() ) {
                    SAnnotSelector::TFeatType t = i->GetFeatType();
                    SAnnotSelector::TFeatSubtype st = i->GetFeatSubtype();
                    NcbiCout << "Feat type: "
                             << setw(10) << CSeqFeatData::SelectionName(t)
                             << " (" << setw(2) << t << ") "
                             << " subtype: "
                             << setw(3) << st
                             << NcbiEndl;
                }
                NcbiCout << "Got feat types in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }
            if ( get_names ) {
                sw.Restart();
                if ( !base_sel.IsIncludedAnyNamedAnnotAccession() ) {
                    NcbiCout << "GB Annot names:" << NcbiEndl;
                    set<string> annot_names =
                        gb_loader->GetNamedAnnotAccessions(idh);
                    ITERATE ( set<string>, i, annot_names ) {
                        NcbiCout << "Named annot: " << *i
                                 << NcbiEndl;
                    }
                }
                else {
                    ITERATE ( list<string>, it, include_named_accs ) {
                        NcbiCout << "GB Annot names for "<<*it<<":" << NcbiEndl;
                        set<string> annot_names =
                            gb_loader->GetNamedAnnotAccessions(idh, *it);
                        ITERATE ( set<string>, i, annot_names ) {
                            NcbiCout << "Named annot: " << *i
                                     << NcbiEndl;
                        }
                    }
                }
                NcbiCout << "Got GB annot names in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
                {{
                    NcbiCout << "All annot names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    SAnnotSelector selt = sel;
                    selt.ResetNamedAnnotAccessions();
                    ITERATE ( list<string>, i, include_named_accs ) {
                        SAnnotSelector sel2 = selt;
                        sel2.IncludeNamedAnnotAccession(*i);
                        CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set,
                                          scope, *range_loc, &sel2);
                        ITERATE ( CFeat_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                            if ( i->IsNamed() ) {
                                NcbiCout << "Named annot: " << i->GetName()
                                         << NcbiEndl;
                            }
                            else {
                                NcbiCout << "Unnamed annot"
                                         << NcbiEndl;
                            }
                        }
                    }
                    NcbiCout << "Got annot names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
                {{
                    NcbiCout << "Feature names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    CFeat_CI it(scope, *range_loc, sel);
                    ITERATE ( CFeat_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                    NcbiCout << "Got feat names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
                {{
                    NcbiCout << "Seq-table names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    CAnnotTypes_CI it(CSeq_annot::C_Data::e_Seq_table, scope, *range_loc, &sel);
                    ITERATE ( CFeat_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                    NcbiCout << "Got table names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
                {{
                    NcbiCout << "Seq-table names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    CSeq_table_CI it(scope, *range_loc, sel);
                    ITERATE ( CSeq_table_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                    NcbiCout << "Got table names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
                {{
                    NcbiCout << "Graph names:" << NcbiEndl;
                    CSeq_loc seq_loc;
                    seq_loc.SetWhole().Set("NC_000001");
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    CGraph_CI it(scope, *range_loc, sel);
                    ITERATE ( CGraph_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                    NcbiCout << "Got graph names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
                {{
                    NcbiCout << "Align names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    sw.Restart();
                    CAlign_CI it(scope, *range_loc, sel);
                    ITERATE ( CAlign_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                    NcbiCout << "Got align names in " << sw.Elapsed() << " secs"
                             << NcbiEndl;
                }}
            }
            continue;
        }

        {{
            if ( count_types ) {
                types_counts.assign(CSeqFeatData::e_MaxChoice, 0);
            }
            if ( count_subtypes ) {
                subtypes_counts.assign(CSeqFeatData::eSubtype_max+1, 0);
            }
            CRef<CSeq_loc_Mapper> mapper;
            if ( handle && print_features && print_mapper ) {
                mapper.Reset(new CSeq_loc_Mapper(handle,
                                                 CSeq_loc_Mapper::eSeqMap_Up));
            }
            if ( handle && args["feat_id"] ) {
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
            if ( handle && args["feat_id_str"] ) {
                string feat_id = args["feat_id_str"].AsString();
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

            int matches = 0, mismatches = 0;
            vector<CConstRef<CSeq_feat> > feats;
            vector<CMappedFeat> mapped_feats;
            vector<CConstRef<CSeq_loc> > mapped_locs;
            
            x_Pause("getting features", pause_key);
            sw.Restart();
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
                if ( get_mapped_feature ) {
                    if ( it->IsSetId() )
                        NcbiCout << MSerial_AsnText << it->GetId();
                    NcbiCout << MSerial_AsnText << it->GetData();
                    if ( it->IsSetPartial() )
                        NcbiCout << "Partial: " << it->GetPartial() << '\n';
                    if ( it->IsSetExcept() )
                        NcbiCout << "Except: " << it->GetExcept() << '\n';
                    if ( it->IsSetComment() )
                        NcbiCout << "Commend: " << it->GetComment() << '\n';
                    if ( it->IsSetProduct() )
                        NcbiCout << "Product: "
                                 << MSerial_AsnText << it->GetProduct();
                    NcbiCout << MSerial_AsnText << it->GetLocation();
                    if ( it->IsSetQual() )
                        ITERATE ( CSeq_feat::TQual, it2, it->GetQual() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetTitle() )
                        NcbiCout << "Title: " << it->GetTitle() << '\n';
                    if ( it->IsSetExt() )
                        NcbiCout << MSerial_AsnText << it->GetExt();
                    //if ( it->IsSetCit() ) NcbiCout << MSerial_AsnText << it->GetCit();
                    if ( it->IsSetExp_ev() )
                        NcbiCout << "Exp-ev: " << it->GetExp_ev() << '\n';
                    if ( it->IsSetXref() )
                        ITERATE ( CSeq_feat::TXref, it2, it->GetXref() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetDbxref() )
                        ITERATE ( CSeq_feat::TDbxref, it2, it->GetDbxref() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetPseudo() )
                        NcbiCout << "Pseudo: " << it->GetPseudo() << '\n';
                    if ( it->IsSetExcept_text() )
                        NcbiCout << "Except-text: "<< it->GetExcept_text() << '\n';
                    /*
                      if ( it->IsSetIds() )
                      ITERATE ( CSeq_feat::TIds, it2, it->GetIds() )
                      NcbiCout << MSerial_AsnText << **it2;
                      if ( it->IsSetExts() )
                      ITERATE ( CSeq_feat::TExts, it2, it->GetExts() )
                      NcbiCout << MSerial_AsnText << **it2;
                    */
                    it->GetMappedFeature();
                }
                if ( sort_seq_feat ) {
                    feats.push_back(ConstRef(&it->GetMappedFeature()));
                }
                if ( save_mapped_feat ) {
                    mapped_feats.push_back(*it);
                    mapped_locs.push_back(ConstRef(&it->GetLocation()));
                }

                if ( table_field.get() &&
                     it->GetSeq_feat_Handle().IsTableFeat() ) {
                    TTableField value;
                    if ( table_field->TryGet(it, value) ) {
                        NcbiCout << "table field: " << value << NcbiEndl;
                    }
                    value = table_field->Get(it);
                }
                
                // Get seq-annot containing the feature
                if ( print_features ) {
                    NcbiCout << "Feature: ";
                    try {
                        NcbiCout << it->GetRange();
                    }
                    catch ( CException& ) {
                        NcbiCout << "multiple id";
                    }
                    if ( it->IsSetPartial() ) {
                        NcbiCout << " partial =" << it->GetPartial();
                    }
                    try {
                        NcbiCout << "\n" <<
                            MSerial_AsnText << it->GetMappedFeature();
                    }
                    catch ( CException& exc ) {
                        ERR_POST("Exception: "<<exc);
                    }
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
                        CSeq_id_Handle loc_id = it->GetLocationId();
                        if ( loc_id ) {
                            NcbiCout << loc_id;
                        }
                        else {
                            NcbiCout << "NULL";
                        }
                        NcbiCout << NcbiEndl;
                    }
                    else {
                        NcbiCout << "Location:\n" <<
                            MSerial_AsnText << it->GetLocation();
                    }
                }

                if ( modify ) {
                    it.GetAnnot().GetEditHandle();
                }
                if ( handle && print_features &&
                     it->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA &&
                     it->IsSetProduct() ) {
                    using namespace sequence;
                    if ( modify ) {
                        handle.GetEditHandle();
                    }
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
                if ( print_features &&
                     it->GetFeatType() == CSeqFeatData::e_Cdregion ) {
                    using namespace sequence;
                    CConstRef<CSeq_feat> gene =
                        GetBestOverlappingFeat(it->GetLocation(),
                                               CSeqFeatData::e_Gene,
                                               eOverlap_Contained,
                                               scope);
                    NcbiCout << "GetBestGeneForCds: "<<it->GetLocation();
                    if ( gene ) {
                        NcbiCout << MSerial_AsnText << *gene;
                        NcbiCout << " compare: " <<
                            MSerial_AsnText << gene->GetLocation() <<
                            "\n with: "<< it->GetOriginalFeature().GetLocation() <<
                            "\n = " << sequence::Compare(gene->GetLocation(),
                                                         it->GetOriginalFeature().GetLocation(),
                                                         &scope,
                                                         sequence::fCompareOverlapping);
                    }
                    else {
                        NcbiCout << "null";
                    }
                    NcbiCout << NcbiEndl;
                }
                if ( print_features &&
                     it->GetFeatSubtype() == CSeqFeatData::eSubtype_ncRNA ) {
                    using namespace sequence;
                    CConstRef<CSeq_feat> gene =
                        GetBestOverlappingFeat(it->GetLocation(),
                                               CSeqFeatData::e_Gene,
                                               eOverlap_Contained,
                                               scope);
                    NcbiCout << "GetBestGeneForXxx: "<<it->GetLocation();
                    if ( gene ) {
                        NcbiCout << MSerial_AsnText << *gene;
                        NcbiCout << " compare: " <<
                            MSerial_AsnText << gene->GetLocation() <<
                            "\n with: "<< it->GetOriginalFeature().GetLocation() <<
                            "\n = " << sequence::Compare(gene->GetLocation(),
                                                         it->GetOriginalFeature().GetLocation(),
                                                         &scope,
                                                         sequence::fCompareOverlapping);
                    }
                    else {
                        NcbiCout << "null";
                    }
                    NcbiCout << NcbiEndl;
                }

                CSeq_annot_Handle annot = it.GetAnnot();
                if ( get_feat_handle && it->IsPlainFeat() ) {
                    CSeq_feat_Handle fh =
                        scope.GetSeq_featHandle(it->GetOriginalFeature());
                    if ( !fh ) {
                        NcbiCout << "Reverse CSeq_feat_Handle lookup failed."
                                 << NcbiEndl;
                    }
                    else if ( fh.GetOriginalSeq_feat() !=
                              &it->GetOriginalFeature() ) {
                        NcbiCout << "Reverse CSeq_feat_Handle differs: "
                                 << MSerial_AsnText<<*fh.GetOriginalSeq_feat()
                                 << NcbiEndl;
                    }
                }
            }
            NcbiCout << "Feat count (loc range, " << sel_msg << "):\t"
                     << count << " in " << sw.Elapsed() << " secs "
                     << NcbiEndl;
            if ( matches ) {
                NcbiCout << "Matches: "<< matches << NcbiEndl;
            }
            if ( mismatches ) {
                NcbiCout << "Mismatches: "<< mismatches << NcbiEndl;
            }
            if ( sort_seq_feat && !feats.empty() ) {
                NcbiCout << "Sorting " << feats.size() << " features..."
                         << NcbiEndl;
                vector<CConstRef<CSeq_feat> > sorted_feats = feats;
                try {
                    stable_sort(sorted_feats.begin(), sorted_feats.end(), PPtrLess<CConstRef<CSeq_feat> >());
                    if ( sorted_feats != feats ) {
                        NcbiCout << "Sorted features are in another order."
                                 << NcbiEndl;
                        for ( size_t i = 0; i < sorted_feats.size(); ++i ) {
                            if ( feats[i] != sorted_feats[i] ) {
                                NcbiCout << "Feature["<<i<<"]:\n"
                                         << "CFeat_CI: " << MSerial_AsnText << *feats[i]
                                         << " Compare: " << MSerial_AsnText << *sorted_feats[i];
                            }
                        }
                    }
                }
                catch ( exception& exc ) {
                    NcbiCout << "Exception while sorting: " << exc.what()
                             << NcbiEndl;
                }
            }
            if ( save_mapped_feat ) {
                for ( size_t i = 0; i < mapped_feats.size(); ++i ) {
                    NcbiCout << "Saved loc: " << MSerial_AsnText
                             << *mapped_locs[i];
                    NcbiCout << "Saved feat: " << MSerial_AsnText
                             << mapped_feats[i].GetMappedFeature();
                }
            }

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
            if ( make_tree ) {
                feature::CFeatTree feat_tree;
                feat_tree.SetFeatIdMode(feat_id_mode);
                feat_tree.SetSNPStrandMode(snp_strand_mode);
                {{
                    CFeat_CI it(scope, *range_loc, base_sel);
                    feat_tree.AddFeatures(it);
                    NcbiCout << "Added "<<it.GetSize()<<" features."
                             << NcbiEndl;
                }}
                NcbiCout << " Root features: "
                         << feat_tree.GetChildren(CMappedFeat()).size()
                         << NcbiEndl;
                if ( print_tree ) {
                    TOrderedTree tree;
                    TOrderedFeatures all;
                    TOrderedTree by_gene;
                    list<CMappedFeat> q;
                    q.push_back(CMappedFeat());
                    ITERATE ( list<CMappedFeat>, pit, q ) {
                        CMappedFeat parent = *pit;
                        vector<CMappedFeat> cc = 
                            feat_tree.GetChildren(parent);
                        TOrderedFeatures& dst = tree[parent];
                        ITERATE ( vector<CMappedFeat>, cit, cc ) {
                            CMappedFeat child = *cit;
                            TFeatureKey key = s_GetFeatureKey(child);
                            dst.insert(key);
                            all.insert(key);
                            q.push_back(child);
                            CMappedFeat gene1 = feat_tree.GetParent(child, CSeqFeatData::eSubtype_gene);
                            CMappedFeat gene = feat_tree.GetBestGene(child, feat_tree.eBestGene_OverlappedOnly);
                            if ( gene && gene != gene1 ) {
                                if ( !by_gene.count(gene) ) {
                                    by_gene[CMappedFeat()].insert(s_GetFeatureKey(gene));
                                }
                                by_gene[gene].insert(key);
                            }
                        }
                    }
                    size_t cnt = 0;
                    TFeatureIndex index;
                    ITERATE ( TOrderedFeatures, fit, all ) {
                        index[*fit] = cnt;
                        NcbiCout << "Feature "<<cnt<<": " << fit->first;
                        ++cnt;
                    }
                    NcbiCout << "Tree:\n";
                    {
                        NcbiCout << "Root features: ";
                        const TOrderedFeatures& cc = tree[CMappedFeat()];
                        ITERATE ( TOrderedFeatures, cit, cc ) {
                            NcbiCout << " " << index[*cit];
                        }
                        NcbiCout << "\n";
                    }
                    ITERATE ( TOrderedFeatures, fit, all ) {
                        NcbiCout << "Children of "<<index[*fit] << ": ";
                        const TOrderedFeatures& cc = tree[fit->second];
                        ITERATE ( TOrderedFeatures, cit, cc ) {
                            NcbiCout << " " << index[*cit];
                        }
                        NcbiCout << "\n";
                    }
                    NcbiCout << NcbiEndl;
                    {
                        string prefix;
                        NcbiCout << "= Tree =\n";
                        const TOrderedFeatures& cc = tree[CMappedFeat()];
                        ITERATE ( TOrderedFeatures, cit, cc ) {
                            s_PrintTree("", "", tree, *cit, index);
                        }
                        NcbiCout << "= end tree =" << NcbiEndl;
                    }
                    if ( !by_gene.empty() ) {
                        string prefix;
                        NcbiCout << "= By gene =\n";
                        const TOrderedFeatures& cc = by_gene[CMappedFeat()];
                        ITERATE ( TOrderedFeatures, cit, cc ) {
                            s_PrintTree("", "", by_gene, *cit, index);
                        }
                        NcbiCout << "= end by gene =" << NcbiEndl;
                    }
                }
            }
        }}

        if ( !only_features && check_cds ) {
            count = 0;
            // The same region, but restricted feature type:
            // searching for e_Cdregion features only. If the sequence is
            // segmented (constructed), search for features on the referenced
            // sequences in the same top level seq-entry, ignore far pointers.
            SAnnotSelector sel = base_sel;
            sel.SetFeatType(CSeqFeatData::e_Cdregion);
            size_t no_product_count = 0;
            sw.Restart();
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
            NcbiCout << "Feat count (loc range, cds):\t" << count
                     << " in " << sw.Elapsed() << " secs"
                     << NcbiEndl;
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
        
        sw.Restart();
        if ( handle ) {
            for ( CFeat_CI it(handle, range, strand, base_sel); it; ++it ) {
                count++;
            }
            NcbiCout << "Feat count (bh range, " << sel_msg << "):\t"
                     << count << " in " << sw.Elapsed() << " secs"
                     << NcbiEndl;
        }

        if ( !only_features ) {
            if ( handle && whole_tse ) {
                count = 0;
                sw.Restart();
                for (CFeat_CI it(handle.GetTopLevelEntry(), base_sel);
                     it; ++it) {
                    count++;
                }
                NcbiCout << "Feat count        (TSE):\t" << count
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }

            if ( !skip_graphs ) {
                // The same way may be used to iterate aligns and graphs,
                // except that there is no type filter for both of them.
                count = 0;
                sw.Restart();
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
                NcbiCout << "Graph count (loc range):\t" << count
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }

            if ( !skip_alignments ) {
                count = 0;
                // Create CAlign_CI using the current scope and location.
                sw.Restart();
                for (CAlign_CI it(scope, *range_loc, base_sel); it;  ++it) {
                    count++;
                    if ( get_mapped_alignments ) {
                        *it;
                    }
                    if ( print_alignments ) {
                        NcbiCout << MSerial_AsnText << *it;
                        NcbiCout << "Original Seq-align: "
                                 << MSerial_AsnText 
                                 << it.GetOriginalSeq_align();
                    }
                }
                NcbiCout << "Align count (loc range):\t" << count
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }

            if ( true ) {
                count = 0;
                // Create CSeq_table_CI using the current scope and location.
                SAnnotSelector sel = base_sel;
                sel.SetAnnotType(CSeq_annot::C_Data::e_Seq_table);
                sw.Restart();
                for (CAnnot_CI it(scope, *range_loc, sel); it;  ++it) {
                    count++;
                    if ( true ) {
                        CSeq_annot_Handle annot = *it;
                        size_t rows = annot.GetSeq_tableNumRows();
                        if ( annot.IsNamed() ) {
                            NcbiCout << "Named " << annot.GetName() << " ";
                        }
                        NcbiCout << "Seq-table with " << rows << " rows."
                                 << NcbiEndl;
                        if ( args["print_seq_table"] ) {
                            NcbiCout << MSerial_AsnText
                                     << *annot.GetCompleteObject()
                                     << NcbiEndl;
                        }
                        if ( table_field.get() ) {
                            for ( size_t row = 0; row < rows; ++row ) {
                                TTableField value;
                                if ( table_field->TryGet(annot, row, value) ) {
                                    NcbiCout << "table field["<<row<<"]: "
                                             << value << NcbiEndl;
                                }
                            }
                        }
                    }
                }
                NcbiCout << "Table count (loc range):\t" << count 
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }
            if ( true ) {
                count = 0;
                // Create CSeq_table_CI using the current scope and location.
                sw.Restart();
                for (CSeq_table_CI it(scope, *range_loc, base_sel); it;  ++it) {
                    count++;
                    CSeq_annot_Handle annot = it.GetAnnot();
                    if ( args["print_seq_table"] ) {
                        NcbiCout << MSerial_AsnText
                                 << *annot.GetCompleteObject()
                                 << NcbiEndl;
                    }
                    if ( true ) {
                        size_t rows = annot.GetSeq_tableNumRows();
                        if ( annot.IsNamed() ) {
                            NcbiCout << "Named " << annot.GetName() << " ";
                        }
                        NcbiCout << "Seq-table with " << rows << " rows."
                                 << NcbiEndl;
                        {
                            NcbiCout << "Original location: "
                                     << MSerial_AsnText << it.GetOriginalLocation()
                                     << NcbiEndl;
                        }
                        if ( it.IsMapped() ) {
                            NcbiCout << "Mapped location: "
                                     << MSerial_AsnText << it.GetMappedLocation()
                                     << NcbiEndl;
                        }
                        if ( table_field.get() ) {
                            for ( size_t row = 0; row < rows; ++row ) {
                                TTableField value;
                                if ( table_field->TryGet(annot, row, value) ) {
                                    NcbiCout << "table field["<<row<<"]: "
                                             << value << NcbiEndl;
                                }
                            }
                        }
                    }
                }
                NcbiCout << "Table count (loc range):\t" << count
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }

            if ( true ) {
                count = 0;
                // Create CAlign_CI using the current scope and location.
                SAnnotSelector sel = base_sel;
                sel.SetAnnotType(CSeq_annot::C_Data::e_Locs);
                sw.Restart();
                for (CAnnot_CI it(scope, *range_loc, sel); it;  ++it) {
                    count++;
                    NcbiCout << "Locs" << NcbiEndl;
                }
                NcbiCout << "Locs count (loc range):\t" << count
                         << " in " << sw.Elapsed() << " secs"
                         << NcbiEndl;
            }

            if ( !save_NA_prefix.empty() ) {
                set<string> accs =
                    gb_loader->GetNamedAnnotAccessions(idh);
                set<CTSE_Handle::TBlobId> ids;
                ITERATE ( set<string>, nit, accs ) {
                    const string& acc = *nit;
                    NcbiCout << "Named: "<<acc<<NcbiEndl;
                    if ( !NStr::StartsWith(acc, "NA") ) {
                        continue;
                    }
                    SAnnotSelector sel = base_sel;
                    sel.ResetAnnotsNames();
                    sel.IncludeNamedAnnotAccession(acc);
                    sel.AddNamedAnnots(acc);
                    set<CTSE_Handle> tses;
                    for ( CAnnot_CI it(handle, sel); it; ++it ) {
                        CTSE_Handle tse = it->GetTSE_Handle();
                        if ( !ids.insert(tse.GetBlobId()).second ) {
                            continue;
                        }
                        tses.insert(tse);
                        string name = save_NA_prefix+acc;
                        name += "-"+tse.GetBlobId().ToString();
                        NcbiCout << "Saving into "<<name<<NcbiEndl;
                        CNcbiOfstream out(name.c_str());
                        out << MSerial_AsnText << *tse.GetCompleteObject();
                    }
                    ITERATE ( set<CTSE_Handle>, it, tses ) {
                        scope.RemoveFromHistory(*it);
                    }
                }
            }
        }

        if ( handle && scan_seq_map ) {
            for ( CSeqMap_CI it(handle, SSeqMapSelector(CSeqMap::fFindData, 9)); it; ++it ) {
                NcbiCout << " @" << it.GetPosition() << "-"
                         << it.GetEndPosition() << " +" << it.GetLength()
                         << NcbiEndl;
            }
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
                        if ( check_gaps ) {
                            seg.GetRefData();
                        }
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

        ITERATE ( vector<CRef<CPrefetchRequest> >, it, prefetch_snp ) {
            CStdPrefetch::Wait(*it);
            const CPrefetchFeat_CI& seq =
                dynamic_cast<const CPrefetchFeat_CI&>(*(*it)->GetAction());
            NcbiCout << "SNP: " << seq.GetResult().GetSize()
                     << NcbiEndl;
        }
        ITERATE ( vector<CRef<CPrefetchRequest> >, it, prefetch_seq ) {
            CStdPrefetch::Wait(*it);
            const CPrefetchSeqData& seq =
                dynamic_cast<const CPrefetchSeqData&>(*(*it)->GetAction());
            NcbiCout << "Seq_data: " << seq.GetResult().size()
                     << " = " << seq.GetResult().substr(0, 10) << "..."
                     << NcbiEndl;
        }

        if ( handle && args["feat_id"] ) {
            if ( 0 ) {
                CTSE_Handle tse = handle.GetTopLevelEntry().GetTSE_Handle();
                CSeq_feat_Handle feat = tse.GetFeatureWithId
                    (CSeqFeatData::e_not_set, args["feat_id"].AsInteger());
                NcbiCout << "Feature with id " << id;
                if ( print_features ) {
                    NcbiCout << MSerial_AsnText << *feat.GetSeq_feat();
                }
                NcbiCout << NcbiEndl;
            }
            else {
                CTSE_Handle tse = handle.GetTopLevelEntry().GetTSE_Handle();
                CObject_id id; id.SetId(args["feat_id"].AsInteger());
                for ( CFeat_CI it(tse, CSeqFeatData::e_not_set, id); it; ++it ) {
                    CSeq_feat_Handle feat = *it;
                    NcbiCout << "Feature with id " << id;
                    if ( print_features ) {
                        NcbiCout << MSerial_AsnText << *feat.GetSeq_feat();
                    }
                    NcbiCout << NcbiEndl;
                }
            }
        }
        
        if ( handle && modify ) {
            //CTSE_Handle tse = handle.GetTSE_Handle();
            //CBioseq_EditHandle ebh = handle.GetEditHandle();
            CRef<CBioseq> newseq(new CBioseq);
            newseq->Assign(*handle.GetCompleteObject());
            CSeq_entry_Handle seh = handle.GetParentEntry();
            if ( CSeq_entry_Handle pseh = seh.GetParentEntry() ) {
                LOG_POST("Reattaching Bioseq");
                {
                    CBioseq_Handle product_handle = handle;
                    handle.Reset();
                    CBioseq_EditHandle eh(product_handle);
                    eh.Remove();
                }
                _ASSERT(!handle);
                _ASSERT(!seh);
                _ASSERT(pseh);
                _ASSERT(pseh == pseh.GetEditHandle());
                pseh.GetEditHandle().AttachBioseq(*newseq);
            }
            else {
                LOG_POST("Reselecting Bioseq");
                seh.GetEditHandle().SelectNone();
                handle = seh.GetEditHandle().SelectSeq(*newseq);
            }
        }
        if ( dump_seq_id ) {
            size_t bytes = CSeq_id_Mapper::GetInstance()->Dump(cout, CSeq_id_Mapper::eCountTotalBytes);
            cout << "Got CSeq_id_Mapper bytes: "<<bytes<<endl;
            CSeq_id_Mapper::GetInstance()->Dump(cout);
            CSeq_id_Mapper::GetInstance()->Dump(cout, CSeq_id_Mapper::eDumpStatistics);
            CSeq_id_Mapper::GetInstance()->Dump(cout, CSeq_id_Mapper::eDumpAllIds);
            if ( args["reset_scope"] ) {
                scope.ResetHistory();
                handle.Reset();
                cout << "Scope reset" << endl;
                CSeq_id_Mapper::GetInstance()->Dump(cout, CSeq_id_Mapper::eDumpAllIds);
            }
        }

        if ( used_memory_check ) {
            if ( args["reset_scope"] ) {
                scope.ResetHistory();
                handle.Reset();
            }
            exit(0);
        }

        if ( handle && args["reset_scope"] ) {
            scope.RemoveFromHistory(handle);
            _ASSERT(!handle);
            handle.Reset();
            scope.ResetHistory();
        }
    }
    if ( modify ) {
        handle = scope.GetBioseqHandle(idh);
        CBioseq_EditHandle ebh = handle.GetEditHandle();
    }

    NcbiCout << "Done" << NcbiEndl;
    return handle? 0: 1;
}


void CDemoApp::Exit(void)
{
    //CObjectManager::GetInstance()->RevokeDataLoader("GBLOADER");
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
