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
* Author:  Aleksey Grichenko
*
* File Description:
*   Examples of using the C++ object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objectio.hpp>

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

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/desc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/impl/synonyms.hpp>

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
    arg_desc->AddOptionalKey("file", "SeqEntryFile",
                             "file with Seq-Entry to load",
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

    arg_desc->AddDefaultKey("loader", "Loader",
                            "Use specified loaders",
                            CArgDescriptions::eString, "");


    arg_desc->AddFlag("print_features", "print all found features");
    arg_desc->AddFlag("only_features", "do only one scan of features");
    arg_desc->AddFlag("reverse", "reverse order of features");
    arg_desc->AddFlag("no_sort", "do not sort features");
    arg_desc->AddFlag("split", "split record");
    arg_desc->AddDefaultKey("max_feat", "MaxFeat",
                            "Max number of features to iterate",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("depth", "depth",
                            "Max depth of segments to iterate",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddFlag("nosnp", "exclude snp features");
    arg_desc->AddDefaultKey("feat_type", "FeatType",
                            "Type of features to select",
                            CArgDescriptions::eInteger, "-1");
    arg_desc->AddDefaultKey("feat_subtype", "FeatSubType",
                            "Subtype of features to select",
                            CArgDescriptions::eInteger, "-1");

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


class CSplit : public CWriteObjectHook
{
public:
    CSplit(const CSeq_entry& entry);
    ~CSplit();
    
    void Save(const string& prefix);
    
    string suffix;
    
    CObjectOStream* Open(const string& prefix,
                         const char* contents_name) const;
    void Save(CConstRef<CSeq_entry> entry,
              const string& prefix,
              const char* contents_name) const;
    
    static CRef<CSeq_entry> NewSeqEntry(void);
    
    CConstRef<CSeq_entry> entry;

    typedef set< CConstRef<CBioseq> > TSet;
    TSet inst;
    TSet ftable;
    TSet align;
    TSet graph;
    TSet snp;

    void WriteObject(CObjectOStream& out,
                     const CConstObjectInfo& object);
    bool skip;
};


class CWSkipMember : public CWriteClassMemberHook
{
public:
    CWSkipMember(const bool& skip)
        : skip(skip)
        {
        }
    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            if ( !skip ) {
                DefaultWrite(out, member);
            }
        }
    const bool& skip;
};

#if 0
class CWAnnot : public CWriteClassMemberHook
{
public:
    CWAnnot(CSplit& split)
        : split(split)
        {
        }
        
    CSplit& split;
        
    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            const CBioseq& seq =
                *CType<CBioseq>::Get(member.GetClassObject());
            if ( !seq.GetId().empty() ) {
                ITERATE ( CBioseq::TAnnot, it, seq.GetAnnot() ) {
                    const CSeq_annot& annot = **it;
                    switch ( annot.GetData().Which() ) {
                    case CSeq_annot::C_Data::e_Ftable:
                    {
                        CTypeConstIterator<CSeq_feat> fi(annot);
                        if ( fi &&
                             fi->GetData().GetSubtype() ==
                             CSeqFeatData::eSubtype_variation ) {
                            split.snp.insert(CConstRef<CBioseq>(&seq));
                        }
                        else {
                            split.ftable.insert(CConstRef<CBioseq>(&seq));
                        }
                        break;
                    }
                    case CSeq_annot::C_Data::e_Graph:
                        split.graph.insert(CConstRef<CBioseq>(&seq));
                        break;
                    case CSeq_annot::C_Data::e_Align:
                        split.align.insert(CConstRef<CBioseq>(&seq));
                        break;
                    }
                }
            }
            else {
                DefaultWrite(out, member);
            }
        }
};


class CWAnnot_set : public CWAnnot
{
public:
    CWAnnot_set(CSplit& split)
        : CWAnnot(split)
        {
        }
};


class CWAnnot_seq : public CWAnnot
{
public:
    CWAnnot_seq(CSplit& split)
        : CWAnnot(split)
        {
        }
};


class CWSeq_data : public CWriteClassMemberHook
{
public:
    CWSeq_data(CSplit& split)
        : split(split)
        {
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            const CBioseq& seq =
                *CType<CBioseq>::Get(member.GetClassObject());
            if ( !seq.GetId().empty() ) {
                split.inst.insert(CConstRef<CBioseq>(&seq));
            }
            else {
                DefaultWrite(out, member);
            }
        }

    CSplit& split;
};


class CWSeq_ext : public CWriteClassMemberHook
{
public:
    CWSeq_ext(CSplit& split)
        : split(split)
        {
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            const CBioseq& seq =
                *CType<CBioseq>::Get(member.GetClassObject());
            if ( !seq.GetId().empty() ) {
                split.inst.insert(CConstRef<CBioseq>(&seq));
            }
            else {
                DefaultWrite(out, member);
            }
        }

    CSplit& split;
};
#endif

class CWSeq_set : public CWriteClassMemberHook
{
public:
    CWSeq_set(const CSplit& split)
        : split(split), dummy_entry_seq(new CSeq_entry)
        {
            dummy_entry_seq->
                SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
            dummy_entry_seq->
                SetSeq().SetInst().SetMol(CSeq_inst::eMol_not_set);
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            COStreamClassMember m(out, member);
    
            COStreamContainer o(out, member);
    
            ITERATE ( CSplit::TSet, i, split.inst ) {
                seq = *i;
                o << *dummy_entry_seq;
                seq.Reset();
            }
        }

    const CSplit& split;
    CRef<CSeq_entry> dummy_entry_seq;
    CConstRef<CBioseq> seq;
};


class CWSeq_id : public CWriteClassMemberHook
{
public:
    CWSeq_id(const CWSeq_set& seq)
        : seq(seq), gb_id(new CSeq_id)
        {
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            if ( seq.seq ) {
                COStreamClassMember m(out, member);
                
                COStreamContainer o(out, member);
            
                ITERATE ( CBioseq::TId, i, seq.seq->GetId() ) {
                    if ( (*i)->IsGi() ) {
                        gb_id->SetGi((*i)->GetGi());
                        o << *gb_id;
                        return;
                    }
                }

                o << **seq.seq->GetId().begin();
            }
        }

    const CWSeq_set& seq;
    CRef<CSeq_id> gb_id;
};


class CWSeq_data : public CWriteClassMemberHook
{
public:
    CWSeq_data(const CWSeq_set& seq)
        : seq(seq)
        {
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            if ( bool(seq.seq) && seq.seq->GetInst().IsSetSeq_data() ) {
                COStreamClassMember m(out, member);
                out.WriteObject(&seq.seq->GetInst().GetSeq_data(),
                                CType<CSeq_data>::GetTypeInfo());
            }
        }

    const CWSeq_set& seq;
};


class CWSeq_ext : public CWriteClassMemberHook
{
public:
    CWSeq_ext(const CWSeq_set& seq)
        : seq(seq)
        {
        }

    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member)
        {
            if ( bool(seq.seq) && seq.seq->GetInst().IsSetExt() ) {
                COStreamClassMember m(out, member);
                out.WriteObject(&seq.seq->GetInst().GetExt(),
                                CType<CSeq_ext>::GetTypeInfo());
            }
        }

    const CWSeq_set& seq;
};


CSplit::CSplit(const CSeq_entry& entry)
    : entry(&entry)
{
}


CSplit::~CSplit(void)
{
}


void CSplit::WriteObject(CObjectOStream& out,
                         const CConstObjectInfo& object)
{
    CConstRef<CBioseq> seq(CType<CBioseq>::Get(object));
    skip = !seq->GetId().empty();
    if ( skip ) {
        ITERATE ( CBioseq::TAnnot, it, seq->GetAnnot() ) {
            const CSeq_annot& annot = **it;
            switch ( annot.GetData().Which() ) {
            case CSeq_annot::C_Data::e_Ftable:
            {
                CTypeConstIterator<CSeq_feat> fi(annot);
                if ( fi &&
                     fi->GetData().GetSubtype() ==
                     CSeqFeatData::eSubtype_variation ) {
                    snp.insert(seq);
                }
                else {
                    ftable.insert(seq);
                }
                break;
            }
            case CSeq_annot::C_Data::e_Graph:
                graph.insert(seq);
                break;
            case CSeq_annot::C_Data::e_Align:
                align.insert(seq);
                break;
            }
        }
        if ( seq->GetInst().IsSetSeq_data() || seq->GetInst().IsSetExt() ) {
            inst.insert(seq);
        }
    }
    DefaultWrite(out, object);
    skip = false;
}

CObjectOStream* CSplit::Open(const string& prefix,
                             const char* contents_name) const
{
    return CObjectOStream::Open(eSerial_AsnText,
                                prefix+contents_name+".asn");
}


void CSplit::Save(CConstRef<CSeq_entry> entry,
                  const string& prefix,
                  const char* contents_name) const
{
    if ( entry ) {
        auto_ptr<CObjectOStream> out(Open(prefix, contents_name));
        *out << *entry;
    }
}


void CSplit::Save(const string& prefix)
{
    {{
        auto_ptr<CObjectOStream> out(Open(prefix, "core"));
        
        CObjectTypeInfo type;
        type = CType<CBioseq>();
        type.SetLocalWriteHook(*out, this);
        
        CRef<CWSkipMember> wSkip(new CWSkipMember(skip));
        type.FindMember("annot").SetLocalWriteHook(*out, &*wSkip);
        type = CType<CSeq_inst>();
        type.FindMember("seq-data").SetLocalWriteHook(*out, &*wSkip);
        type.FindMember("ext").SetLocalWriteHook(*out, &*wSkip);

        *out << *entry;
    }}

    if ( !inst.empty() ) {
        auto_ptr<CObjectOStream> out(Open(prefix, "seq"));

        CRef<CSeq_entry> dummy_entry(new CSeq_entry);
        dummy_entry->SetSet();

        CRef<CWSeq_set> wSeq_set(new CWSeq_set(*this));
        CRef<CWSeq_id> wSeq_id(new CWSeq_id(*wSeq_set));
        CRef<CWSeq_data> wSeq_data(new CWSeq_data(*wSeq_set));
        CRef<CWSeq_ext> wSeq_ext(new CWSeq_ext(*wSeq_set));
        
        CObjectTypeInfo type = CType<CBioseq_set>();
        type.FindMember("seq-set").SetLocalWriteHook(*out, &*wSeq_set);
        type = CType<CBioseq>();
        type.FindMember("id").SetLocalWriteHook(*out, &*wSeq_id);
        type = CType<CSeq_inst>();
        type.FindMember("seq-data").SetLocalWriteHook(*out, &*wSeq_data);
        type.FindMember("ext").SetLocalWriteHook(*out, &*wSeq_ext);

        *out << *dummy_entry;
    }
#if 0
        CNcbiOstrstream s;
        s << base_name;
        switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
        {
            CTypeConstIterator<CSeq_feat> fi(annot);
            if ( fi &&
                 fi->GetData().GetSubtype() ==
                 CSeqFeatData::eSubtype_variation ) {
                s << "snp." << ++cnt[3];
            }
            else {
                s << "ftable." << ++cnt[0];
            }
            break;
        }
        case CSeq_annot::C_Data::e_Graph:
            s << "graph." << ++cnt[1];
            break;
        case CSeq_annot::C_Data::e_Align:
            s << "align." << ++cnt[2];
            break;
        }
        s << ".asn";
        string file_name = CNcbiOstrstreamToString(s);
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, file_name));
        *out << annot;
    }

    for ( CTypeConstIterator<CSeq_annot> i(entry); i; ++i ) {
        const CSeq_annot& annot = *i;
        CNcbiOstrstream s;
        s << base_name;
        switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
        {
            CTypeConstIterator<CSeq_feat> fi(annot);
            if ( fi &&
                 fi->GetData().GetSubtype() ==
                 CSeqFeatData::eSubtype_variation ) {
                s << "snp." << ++cnt[3];
            }
            else {
                s << "ftable." << ++cnt[0];
            }
            break;
        }
        case CSeq_annot::C_Data::e_Graph:
            s << "graph." << ++cnt[1];
            break;
        case CSeq_annot::C_Data::e_Align:
            s << "align." << ++cnt[2];
            break;
        }
        s << ".asn";
        string file_name = CNcbiOstrstreamToString(s);
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, file_name));
        *out << annot;
    

    Save(inst, prefix, "seq");
    Save(ftable, prefix, "ftable");
    Save(align, prefix, "align");
    Save(graph, prefix, "graph");
    for ( size_t i = 0; i < snp.size(); ++i ) {
        CNcbiOstrstream name_out;
        name_out << "snp."<<(i+1);
        string name = CNcbiOstrstreamToString(name_out);
        auto_ptr<CObjectOStream> out(Open(prefix, name.c_str()));
        *out << *snp[i];
    }
#endif
}

int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id;
    int gi;
    if ( args["gi"] ) {
        gi = args["gi"].AsInteger();
        id.Reset(new CSeq_id);
        id->SetGi(gi);
    }
    else if ( args["id"] ) {
        id.Reset(new CSeq_id(args["id"].AsString()));
        gi = -1;
    }
    else {
        ERR_POST(Fatal << "Either -gi or -id argument is required");
    }

    CFeat_CI::EResolveMethod resolve = CFeat_CI::eResolve_TSE;
    if ( args["resolve"].AsString() == "all" )
        resolve = CFeat_CI::eResolve_All;
    if ( args["resolve"].AsString() == "none" )
        resolve = CFeat_CI::eResolve_None;
    if ( args["resolve"].AsString() == "tse" )
        resolve = CFeat_CI::eResolve_TSE;
    if ( !args["loader"].AsString().empty() ) {
        string env = "GENBANK_LOADER_METHOD="+args["loader"].AsString();
        ::putenv(::strdup(env.c_str()));
    }

    int repeat_count = args["count"].AsInteger();
    int pause = args["pause"].AsInteger();
    bool only_features = args["only_features"];
    bool print_features = args["print_features"];
    bool split = args["split"];
    SAnnotSelector::ESortOrder order =
        args["reverse"] ?
        SAnnotSelector::eSortOrder_Reverse : SAnnotSelector::eSortOrder_Normal;
    if ( args["no_sort"] )
        order = SAnnotSelector::eSortOrder_None;
    int max_feat = args["max_feat"].AsInteger();
    int depth = args["depth"].AsInteger();
    int feat_type = args["feat_type"].AsInteger();
    int feat_subtype = args["feat_subtype"].AsInteger();
    bool nosnp = args["nosnp"];

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm(new CObjectManager);

    // Create genbank data loader and register it with the OM.
    // The last argument "eDefault" informs the OM that the loader
    // must be included in scopes during the CScope::AddDefaults() call.
    pOm->RegisterDataLoader(
        *new CGBDataLoader("ID", 0, 2),
        CObjectManager::eDefault);

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(eSerial_AsnText,
                                  args["file"].AsInputFile()));
        *in >> *entry;
        scope.AddTopLevelSeqEntry(*entry);
    }

    // Get bioseq handle for the seq-id. Most of requests will use this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(*id);
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found");
    }
    if ( gi < 0 ) {
        CConstRef<CSynonymsSet> syns = scope.GetSynonyms(handle);
        ITERATE ( CSynonymsSet, it, *syns ) {
            const CSeq_id& seq_id = it->GetSeqId();
            if ( seq_id.Which() == CSeq_id::e_Gi ) {
                gi = seq_id.GetGi();
                break;
            }
        }
    }

    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c && pause ) {
            SleepSec(pause);
        }
        string sout;
        int count;
        if ( c == 0 && split ) {
            CSplit s(handle.GetTopLevelSeqEntry());
            CNcbiOstrstream name;
            name << gi << '.';
            s.Save(CNcbiOstrstreamToString(name));
            break;
        }
        if ( !only_features ) {
            // List other sequences in the same TSE
            NcbiCout << "TSE sequences:" << NcbiEndl;
            CBioseq_CI bit;
            bit = CBioseq_CI(scope, handle.GetTopLevelSeqEntry());
            for ( ; bit; ++bit) {
                NcbiCout << "    " << bit->GetSeqId()->DumpAsFasta() << NcbiEndl;
            }

            // Get the bioseq
            CConstRef<CBioseq> bioseq(&handle.GetBioseq());
            // -- use the bioseq: print the first seq-id
            NcbiCout << "First ID = " <<
                (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

            // Get the sequence using CSeqVector. Use default encoding:
            // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
            CSeqVector seq_vect = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            // -- use the vector: print length and the first 10 symbols
            NcbiCout << "Sequence: length=" << seq_vect.size();
            if (seq_vect.CanGetRange(0, seq_vect.size() - 1)) {
                NcbiCout << " data=";
                sout = "";
                for (TSeqPos i = 0; (i < seq_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += seq_vect[i];
                }
                NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
            }
            else {
                NcbiCout << " data unavailable" << NcbiEndl;
            }
            // CSeq_descr iterator: iterates all descriptors starting
            // from the bioseq and going the seq-entries tree up to the
            // top-level seq-entry.
            count = 0;
            for (CDesc_CI desc_it(handle);
                 desc_it;  ++desc_it) {
                count++;
            }
            NcbiCout << "Desc count: " << count << NcbiEndl;

            count = 0;
            for (CSeq_annot_CI ai(scope, handle.GetTopLevelSeqEntry());
                 ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (recursive): " << count << NcbiEndl;

            count = 0;
            for (CSeq_annot_CI ai(scope,
                                  handle.GetTopLevelSeqEntry(),
                                  CSeq_annot_CI::eSearch_entry);
                 ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (non-recursive): "
                     << count << NcbiEndl;
        }

        // CSeq_feat iterator: iterates all features which can be found in the
        // current scope including features from all TSEs.
        // Construct seq-loc to get features for.
        CSeq_loc loc;
        // No region restrictions -- the whole bioseq is used:
        loc.SetWhole().SetGi(gi);
        count = 0;
        // Create CFeat_CI using the current scope and location.
        // No feature type restrictions.
        SAnnotSelector base_sel;
        base_sel
            .SetResolveMethod(resolve)
            .SetSortOrder(order)
            .SetMaxSize(max_feat)
            .SetResolveDepth(depth);
        if ( nosnp ) {
            base_sel.SetDataSource("");
        }

        {{
            SAnnotSelector sel = base_sel;
            if ( feat_type >= 0 ) {
                sel.SetFeatChoice(SAnnotSelector::TFeatChoice(feat_type));
            }
            if ( feat_subtype >= 0 ) {
                sel.SetFeatSubtype(SAnnotSelector::TFeatSubtype(feat_subtype));
            }
            CFeat_CI it(scope, loc, sel);
            for ( ; it;  ++it) {
                count++;
                // Get seq-annot containing the feature
                if ( print_features ) {
                    auto_ptr<CObjectOStream>
                        out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                    *out << it->GetMappedFeature();
                    *out << it->GetLocation();
                }
                CConstRef<CSeq_annot> annot(&it.GetSeq_annot());
            }
            if ( feat_type >= 0 || feat_subtype >= 0 ) {
                NcbiCout << "Feat count (whole, requested): ";
            }
            else {
                NcbiCout << "Feat count (whole, any):       ";
            }
            NcbiCout << count << NcbiEndl;
            _ASSERT(count == (int)it.GetSize());
        }}

        if ( only_features )
            continue;

        {
            count = 0;
            // The same region (whole sequence), but restricted feature type:
            // searching for e_Cdregion features only. If the sequence is
            // segmented (constructed), search for features on the referenced
            // sequences in the same top level seq-entry, ignore far pointers.
            SAnnotSelector sel = base_sel;
            sel.SetFeatChoice(CSeqFeatData::e_Cdregion);
            for ( CFeat_CI it(scope, loc, sel); it;  ++it ) {
                count++;
                // Get seq vector filtered with the current feature location.
                // e_ViewMerged flag forces each residue to be shown only once.
                CSeqVector cds_vect = handle.GetSequenceView
                    (it->GetLocation(), CBioseq_Handle::eViewMerged,
                     CBioseq_Handle::eCoding_Iupac);
                // Print first 10 characters of each cd-region
                NcbiCout << "cds" << count << " len=" << cds_vect.size() << " data=";
                if ( cds_vect.size() == 0 ) {
                    NcbiCout << "Zero size from: ";
                    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                    *out << it->GetOriginalFeature().GetLocation();
                    out->Flush();
                    NcbiCout << "Zero size to: ";
                    *out << it->GetMappedFeature().GetLocation();
                }
                sout = "";
                for (TSeqPos i = 0; (i < cds_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += cds_vect[i];
                }
                NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
            }
            NcbiCout << "Feat count (whole, cds):      " << count << NcbiEndl;
        }

        // Region set to interval 0..9 on the bioseq. Any feature
        // intersecting with the region should be selected.
        loc.SetInt().SetId().SetGi(gi);
        loc.SetInt().SetFrom(0);
        loc.SetInt().SetTo(9);
        count = 0;
        // Iterate features. No feature type restrictions.
        for (CFeat_CI it(scope, loc, base_sel); it;  ++it) {
            count++;
        }
        NcbiCout << "Feat count (int. 0..9, any):   " << count << NcbiEndl;

        // Search features only in the TSE containing the target bioseq.
        // Since only one seq-id may be used as the target bioseq, the
        // iterator is constructed not from a seq-loc, but from a bioseq handle
        // and start/stop points on the bioseq. If both start and stop are 0 the
        // whole bioseq is used. The last parameter may be used for type filtering.
        count = 0;
        for ( CFeat_CI it(handle, 0, 999, base_sel); it;  ++it ) {
            count++;
            if ( print_features ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << it->GetMappedFeature();
                *out << it->GetLocation();
            }
        }
        NcbiCout << "Feat count (bh, 0..999, any):  " << count << NcbiEndl;

        // The same way may be used to iterate aligns and graphs,
        // except that there is no type filter for both of them.
        // No region restrictions -- the whole bioseq is used:
        loc.SetWhole().SetGi(gi);
        count = 0;
        for (CGraph_CI it(scope, loc,
                          SAnnotSelector()
                          .SetResolveMethod(resolve)
                          .SetSortOrder(order)); it;  ++it) {
            count++;
            // Get seq-annot containing the feature
            if ( print_features ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << it->GetMappedGraph();
                *out << it->GetLoc();
            }
            CConstRef<CSeq_annot> annot(&it.GetSeq_annot());
        }
        NcbiCout << "Graph count (whole, any):       " << count << NcbiEndl;

        count = 0;
        // Create CAlign_CI using the current scope and location.
        for (CAlign_CI align_it(scope, loc); align_it;  ++align_it) {
            count++;
        }
        NcbiCout << "Annot count (whole, any):       " << count << NcbiEndl;
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
    return CDemoApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
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

