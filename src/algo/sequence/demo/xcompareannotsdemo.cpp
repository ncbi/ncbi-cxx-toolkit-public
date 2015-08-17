/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 ..
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
 * Author:  Alex Astashyn
 *
 * File Description:
 *   Annotation cross-comparator
 *
 */

#include <ncbi_pch.hpp>

#include <vector>
#include <map>
#include <list>


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/plugin_manager.hpp>


#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objects/seqset/gb_release_file.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>

#include <objtools/alnmgr/alnmix.hpp>

#include <algo/sequence/loc_mapper.hpp>
#include <algo/sequence/compare_feats.hpp>

//#include <db/bdb/bdb_blobcache.hpp>
#include <dbapi/driver/drivers.hpp>

#include <limits>

USING_NCBI_SCOPE ;
USING_SCOPE (objects);

///////////////////////////////////////////////////////////////////////////////


//Remap intervals individually and compute
//the quality of the remapping (identity)
class CLocMapper_Default : public ILocMapper
{
public:

    class CGappedRange
    {
    public:
        typedef CSeq_loc::TRange TRange;
        CGappedRange(TRange range = TRange(), unsigned gaps = 0) : m_range(range), m_gaps(gaps) {}
        CGappedRange(const CGappedRange& r1, const CGappedRange& r2)
        {
            TSeqPos r1_len = r1.m_range.GetLength();
            TSeqPos r2_len = r2.m_range.GetLength();

            m_range = r1.m_range.CombinationWith(r2.m_range);
            m_gaps = r1.m_gaps + r2.m_gaps + (m_range.GetLength() - (r1_len + r2_len));
        }

        string ToString() const
        {
            return "GRange{ " + NStr::UInt8ToString(m_range.GetFrom()) + "\t"
                +  NStr::UInt8ToString(m_range.GetTo()) + "\t"
                +  NStr::UInt8ToString(m_range.GetLength()) + "\t"
                +  NStr::UInt8ToString(m_gaps) + "\t"
                +  NStr::DoubleToString(GetIdentity()) + "}";

        }

        double GetIdentity() const
        {
            return m_range.GetLength() == 0 ? 0 : 1.0 - static_cast<double>(m_gaps) / m_range.GetLength();
        }

        TRange m_range;
        unsigned m_gaps;
    };


    /* this represents the quality of the remapping;
     */
    static double GetBiasedSymmetricIdentity(
            TSeqPos qry_len,
            TSeqPos tgt_len,
            TSeqPos aln_len,
            double qry_bias = 0.5)
    {
        double d_aln_len = static_cast<double>(aln_len);
        return (qry_len == 0 ? 0.0 : d_aln_len/(qry_len) * qry_bias)
             + (tgt_len == 0 ? 0.0 : d_aln_len/(tgt_len) * (1.0 - qry_bias));
    }

    CGappedRange CollapseRanges(list<CGappedRange>& ranges, TSeqPos query_len, double* identity_out = NULL)
    {
        CGappedRange r;
        int pos = 0;
        int best_ending_pos = ranges.size() - 1;

        r = CGappedRange();
        pos = ranges.size() - 1;
        int best_beginning_pos = 0;


        r = CGappedRange();
        pos = 0;
        ITERATE(list<CGappedRange>, it, ranges) {
            if(pos >= best_beginning_pos && pos <= best_ending_pos) {
                r = CGappedRange(r, *it);
            }
            pos++;
        }

        if(identity_out != NULL) {
            unsigned aligned_len = r.m_range.GetLength() - r.m_gaps;
            unsigned d = (query_len + r.m_range.GetLength() - aligned_len);
            *identity_out = d == 0 ? 0 : static_cast<double>(aligned_len) / d;
        }

        return r;
    }


    //we might need to remap locations with seq-ids specified as accession-noversion.
    //We must treat such seq-id as the same version in the mapper's underlying alignment
    //as opposed to latest-version-in-id. We keep the from_id that contains
    //the accession-version in the alignment query, so if we encounter
    //an versionless-seq-id location we'll treat it as that versioned seq-id
    CLocMapper_Default(
            CSeq_loc_Mapper& mapper,
            CScope& scope,
            bool is_spliced = false,    //if true, do not collapse remapped ranges
            const CSeq_id* from_id = NULL,
            bool strip_versions = false)
        : m_scope(scope)
        , m_is_spliced(is_spliced)
        , m_from_id(new CSeq_id)
        , m_from_id__accession("")
        , m_mapper(mapper)
        , m_strip_versions(strip_versions)
     {
        if(from_id && !from_id->IsLocal()) {
            m_from_id->Assign(*from_id);
            m_from_id__accession = sequence::GetAccessionForId(*m_from_id, m_scope, sequence::eWithoutAccessionVersion);
        }
     }


    CRef<CSeq_loc> Map(const CSeq_loc& loc, double* mapped_identity = NULL)
    {
        CRef<CSeq_loc> temp_loc(new CSeq_loc);
        CRef<CSeq_loc> mapped_loc(new CSeq_loc);
        mapped_loc->SetMix();

        temp_loc->Assign(loc);
        temp_loc->ChangeToMix();

        TSeqPos query_len_total(0);
        TSeqPos aligned_len_total(0);
        TSeqPos collapsed_len_total(0);

        for(CSeq_loc_CI it(*temp_loc); it; ++it) {
            CConstRef<CSeq_loc> ci_loc = it.GetRangeAsSeq_loc();
            TSeqPos query_len = sequence::GetLength(*ci_loc, NULL); //may belong to another scope
            query_len_total += query_len;

            CRef<CSeq_loc> mapped_interval = m_mapper.Map(*ci_loc);

            if(mapped_interval->IsNull() || mapped_interval->IsEmpty()) {
                continue;
            }

            CRef<CSeq_loc> mapped_interval_merged = sequence::Seq_loc_Merge(
                    *mapped_interval,
                    CSeq_loc::fMerge_All,
                    &m_scope);
            //TSeqPos remapped_len = sequence::GetLength(*mapped_interval_merged, NULL);


            //in case of spliced alignments we remap an interval to
            //a gapped loc; In case of non-spliced alignments we
            //remap each interval indivdually and collapse it to single range.
            //In case of a spliced alignment it would collapse the whole thing
            //into one range, so we have to process them differently
            if(m_is_spliced) {
                mapped_loc->Add(*mapped_interval_merged);
                continue;
            }



            list<CGappedRange> mapped_ranges_list;
            for(CSeq_loc_CI it2(*mapped_interval_merged); it2; ++it2) {
                mapped_ranges_list.push_back(CGappedRange(it2.GetRange()));
            }



            CGappedRange r = CollapseRanges(
                    mapped_ranges_list,
                    query_len);

            TSeqPos collapsed_len = r.m_range.GetLength();
            TSeqPos aligned_len = collapsed_len - r.m_gaps;


            aligned_len_total += aligned_len;
            collapsed_len_total += collapsed_len;

            if(aligned_len > query_len) {
                //The identity formula relies on non-redundant remapping
                string s2 = "";
                ci_loc->GetLabel(&s2);
                string s3 = "";
                mapped_interval->GetLabel(&s3);
                ERR_POST(Warning << "Detected non-redundant remapping from\n"
                                 //<< s0
                                 //<< "\nto\n"
                                 //<< s1
                                 << "\nsegment\n"
                                 << s2
                                 << "\nremapped segment\n"
                                 << s3
                                 << "\naligned len: " << aligned_len
                                 << "\ncollapsed len: " << collapsed_len
                );
            }

            CRef<CSeq_loc> mapped_interval_collapsed(new CSeq_loc);
            mapped_interval_collapsed->SetInt().SetFrom(r.m_range.GetFrom());
            mapped_interval_collapsed->SetInt().SetTo(r.m_range.GetTo());
            mapped_interval_collapsed->SetInt().SetStrand(sequence::GetStrand(*mapped_interval_merged, &m_scope));

            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(sequence::GetId(*mapped_interval_merged, &m_scope));
            mapped_interval_collapsed->SetInt().SetId(*id);

            mapped_loc->Add(*mapped_interval_collapsed);
        }


        if(mapped_identity != NULL) {
            if(m_is_spliced) {
                *mapped_identity = 1;
            } else {
                TSeqPos d = query_len_total + collapsed_len_total - aligned_len_total;
                *mapped_identity =  d == 0 ? 0 : static_cast<double>(aligned_len_total) / d;
            }

        }

        return mapped_loc;
    }


private:
    CScope& m_scope;
    bool m_is_spliced;
    CRef<CSeq_id> m_from_id;
    string m_from_id__accession;
    CSeq_loc_Mapper& m_mapper;
    bool m_strip_versions;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class CGbScopeLoader : public CGBReleaseFile::ISeqEntryHandler
{
public:
    CGbScopeLoader(CScope& scope) : m_scope(scope) {};
    virtual ~CGbScopeLoader() {}

    bool HandleSeqEntry(CRef<CSeq_entry>& entry)
    {
        m_scope.AddTopLevelSeqEntry(*entry);
        return true;
    }
private:
    CScope& m_scope;
};

bool IsManualScope(CScope& scope) {
    CScope::TTSE_Handles handles;
    scope.GetAllTSEs(handles, CScope::eManualTSEs);
    int k = handles.size();
    return k != 0;
}

bool IsInScope(CScope& scope, const CSeq_id& id)
{
    //if(scope.GetBioseqHandle(id)) {
    //    return true;
    //}

    //this used to be implemented by checking if bioseq handle could be retreived,
    //(as above)
    //but depending on what kind of scope we're dealing with and how it is indexed,
    //this may not be reliable. So instead we check by trying to see if we can
    //find any features at all
    SAnnotSelector sa;

    sa.SetResolveAll();
    if(IsManualScope(scope)) {
        sa.SetSearchUnresolved();
    }

    sa.SetMaxSize(1);

    sa.SetSortOrder(SAnnotSelector::eSortOrder_None);

    sa.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
    sa.IncludeFeatType(CSeqFeatData::e_Rna);
    sa.IncludeFeatType(CSeqFeatData::e_Cdregion);
    sa.IncludeFeatType(CSeqFeatData::e_Gene);

    CRef<CSeq_loc> loc(new CSeq_loc);

    CRef<CSeq_id> seq_id(new CSeq_id);
    seq_id->Assign(id);
    loc->SetWhole(*seq_id);



    for(CFeat_CI ci(scope, *loc, sa); ci; ++ci) {
        return true;
    }

    return false;
}

string GetProductLabel(const CSeq_feat& feat, CScope& scope)
{
    string s = "";
    if(feat.CanGetProduct()) {
        if(sequence::GetId(feat.GetProduct(), &scope).IsGi()) {
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->Assign(feat.GetProduct());
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(*sequence::GetId(
                    sequence::GetId(*loc, &scope),
                    scope,
                    sequence::eGetId_ForceAcc).GetSeqId());

            loc->SetId(*id);
            loc->GetLabel(&s);
        } else {
            feat.GetProduct().GetLabel(&s);
        }
    }
    return s;
}

ESerialDataFormat StringToSerialFormat(string str)
{
    return
        str == "asn_text" ? eSerial_AsnText :
        str == "asn_bin"  ? eSerial_AsnBinary :
        str == "asn_xml"  ? eSerial_Xml :
        str == "asn_json" ? eSerial_Json :
        eSerial_None;
}

string GetIdStartStopStrandStr(
        const CSeq_loc& loc,
        CScope& scope,
        bool use_long_label)
{
    string long_label = "";
    if(use_long_label) {
        loc.GetLabel(&long_label);
        long_label += "\t";
    }

    //NcbiCout << MSerial_AsnText << loc;
    CRef<CSeq_loc> tmp_loc = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, &scope);

    string out = "";
    try {
        out = sequence::GetId(
                sequence::GetId(*tmp_loc, &scope),
                scope,
                sequence::eGetId_ForceAcc).AsString();
    } catch(...) {
        out = sequence::GetId(*tmp_loc, &scope).AsFastaString();
    }

    if(loc.IsWhole()) {
        out += "\t\t";
    } else {
        out += "\t" + NStr::UInt8ToString(sequence::GetStart(*tmp_loc, &scope) + 1);
        out += "\t" + NStr::UInt8ToString(sequence::GetStop(*tmp_loc, &scope) + 1);
    }

    out += "\t"; out += (sequence::GetStrand(*tmp_loc, &scope) == eNa_strand_minus ? "-" : "+" );

    return long_label + out;
}


void AddSpanningSentinelFeat(
        CScope& scope,
        const CSeq_id& id,
        CSeqFeatData::E_Choice type,
        string title = "Sentinel")
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    //entry->SetSeq().SetAnnot().push_back(annot);
    CRef<CSeq_id> new_id(new CSeq_id);
    new_id->Assign(id);


    CRef<CSeq_feat> feat(new CSeq_feat);
    annot->SetData().SetFtable().push_back(feat);
    feat->SetTitle(title);

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*new_id);
    feat->SetLocation(*loc);
    feat->SetData().Select(type);



    if(type == CSeqFeatData::e_Rna) {
        CRef<CRNA_ref> rna_ref(new CRNA_ref);
        rna_ref->SetType(CRNA_ref::eType_other);
        feat->SetData().SetRna(*rna_ref);
    } else if(type == CSeqFeatData::e_Gene) {
        CRef<CGene_ref> gene_ref(new CGene_ref);
        feat->SetData().SetGene();
    } else {
        NCBI_THROW(CException, eUnknown, "Type must be eGene, eRna");
    }

    scope.AddSeq_annot(*annot);
}


/*
 * Sometimes we compare alignment of some seq to annotation, and the source
 * seq is an arbitrary fasta without annotation. We'll create a sentinel seq,
 * add it to scope, and have a spanning RNA feat on it, so then we can
 * compare it as usual
 */
void AddSentinelRNASeq(CScope& scope, const CSeq_id& id)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CRef<CSeq_id> new_id(new CSeq_id);
    new_id->Assign(id);

    entry->SetSeq().SetId().push_back(new_id);
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    entry->SetSeq().SetInst().SetLength(1000000); //othrewise CFeat_CI throws
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna); //mol type required by mapper to compute seq width

    scope.ResetHistory(); //otherwise adding new entry to non-clean scope produces warnings
    scope.AddTopLevelSeqEntry(*entry);

    AddSpanningSentinelFeat(scope, id, CSeqFeatData::e_Gene);
    AddSpanningSentinelFeat(scope, id, CSeqFeatData::e_Rna);
}


/* If the sequence has mRNA in title but no mrna annotated, add spanning
 * mrna feature for the purpose of comparisons. Same goes for Gene
 */
void AddDefaultSentinelFeats(CScope& scope, const CSeq_loc& loc)
{
    const CSeq_id& id = sequence::GetId(loc, &scope);
    CBioseq_Handle h = scope.GetBioseqHandle(id);
    string title = sequence::CDeflineGenerator().GenerateDefline(h);

    bool add_gene = false;
    bool add_rna = false;
    bool is_gene = false;

    SAnnotSelector sel;
    sel.SetResolveAll();
    sel.SetOverlapTotalRange();
    sel.SetResolveDepth(0);

    CMolInfo::TBiomol biomol =  CMolInfo::eBiomol_unknown;
    try {
        biomol = sequence::GetMolInfo(h)->GetBiomol();
    } catch (CException&) {};

    bool biomol_rna = !(   biomol == CMolInfo::eBiomol_unknown
                           || biomol == CMolInfo::eBiomol_genomic
                           || biomol == CMolInfo::eBiomol_peptide
                           || biomol == CMolInfo::eBiomol_other_genetic
                           || biomol == CMolInfo::eBiomol_other);



    if(    NStr::Find(title, "gene", 0, NPOS, NStr::eFirst, NStr::eNocase) != NPOS
        || NStr::Find(title, "mRNA", 0, NPOS, NStr::eFirst, NStr::eNocase) != NPOS
        || NStr::Find(title, "CDS",  0, NPOS, NStr::eFirst, NStr::eNocase) != NPOS)
    {
        is_gene = true;
    }

    //we will add a gene feature if missing if:
    //biomol is rna
    //or biomol is not rna BUT the title contains gene|mRNA|CDS
    //(this happens when genes are submitted on genomic sequences and may have gene feature missing)

    if(is_gene || biomol_rna) {
        sel.SetFeatType(CSeqFeatData::e_Gene);
        add_gene = true;
        for(CFeat_CI ci(scope, loc, sel); ci; ++ci) {
            add_gene = false; break;
        }

        if(add_gene) {
            AddSpanningSentinelFeat(
                    scope,
                    id,
                    CSeqFeatData::e_Gene,
                    "[Sentinel feat]" + title);
        }

    }

    if(biomol_rna) {
        sel.SetFeatType(CSeqFeatData::e_Rna);
        add_rna = true;
        for(CFeat_CI ci2(scope, loc, sel); ci2; ++ci2) {
            add_rna = false; break;
        }

        if(add_rna) {
            AddSpanningSentinelFeat(
                    scope,
                    id,
                    CSeqFeatData::e_Rna,
                    "[Sentinel feat]" + title);
        }
    }

}

auto_ptr<CObjectIStream> GetIStream(string path, ESerialDataFormat serial_format)
{
    auto_ptr<CObjectIStream> obj_istr;
    if ( NStr::EndsWith(path, ".gz")) {
        obj_istr.reset(CObjectIStream::Open(
            serial_format,
            *(new CCompressionIStream(
                *(new CNcbiIfstream(path.c_str(), IOS_BASE::in | IOS_BASE::binary)),
                new CZipStreamDecompressor(CZipCompression::fCheckFileHeader),
                CCompressionStream::fOwnAll)),
            eTakeOwnership));
    } else {
        obj_istr.reset(CObjectIStream::Open(serial_format, path));
    }

    return obj_istr;
}


//Note: we need to return the method because depending on the method
//we will decide whether to use scope object for remapping.
enum ELoadScopeMethod {eLoadScope_Failed, eLoadScope_SeqEntry, eLoadScope_SeqAnnot, eLoadScope_GBR};
typedef int TLoadScopeMethod;
TLoadScopeMethod LoadScope(string arg_path, CScope& scope, ESerialDataFormat serial_format)
{
    AutoPtr<CDirEntry> file_or_dir(new CDirEntry(arg_path));

    CDir::TEntries dir_entries;
    if(file_or_dir->IsDir()) {
        CDir dir(*file_or_dir);
        dir_entries = dir.GetEntries();
    } else {
        dir_entries.push_back(file_or_dir);
    }

    TLoadScopeMethod method = eLoadScope_Failed;

    ITERATE(CDir::TEntries, it, dir_entries) {
        AutoPtr<CDirEntry> ent = *it;
        string path = ent->GetPath();


        if(!ent->IsFile()) continue;

        _TRACE("loading " + path);
        try {
            auto_ptr<CObjectIStream> obj_istr = GetIStream(path, serial_format);
            if (!obj_istr->InGoodState()) {
                ERR_POST(Error << "Could not open file " << *it);
                return eLoadScope_Failed;
            }
            _TRACE("Trying as Seq-entry");

            while(!obj_istr->EndOfData()) {
                CRef<CSeq_entry> seq_entry(new CSeq_entry);
                *obj_istr >> *seq_entry;
                _TRACE("adding TSE from " + path);
                scope.AddTopLevelSeqEntry(*seq_entry);
            }
            _TRACE("Loaded as Seq-entry");
            method = eLoadScope_SeqEntry;
            continue;
        } catch(CException&) {};

        try {
            auto_ptr<CObjectIStream> obj_istr = GetIStream(path, serial_format);
            if (!obj_istr->InGoodState()) {
                ERR_POST(Error << "Could not open file " << *it);
                return false;
            }
            _TRACE("Trying as Seq-annot");

            while(!obj_istr->EndOfData()) {
                CRef<CSeq_annot> seq_annot(new CSeq_annot);
                *obj_istr >> *seq_annot;
                _TRACE("adding Seq-annot from " + path);
                scope.AddSeq_annot(*seq_annot);
            }
            _TRACE("Loaded as Seq-annot");
            method = eLoadScope_SeqAnnot;
            continue;
        } catch(CException&) {};

        try {
            auto_ptr<CObjectIStream> obj_istr = GetIStream(path, serial_format);
            if (!obj_istr->InGoodState()) {
                ERR_POST(Error << "Could not open file " << *it);
                return eLoadScope_Failed;;
            }

            _TRACE("Trying as genbank bioseqset");
            CGBReleaseFile rf(*obj_istr.release());
            rf.RegisterHandler(new CGbScopeLoader(scope));
            rf.Read();
            _TRACE("Loaded as genbank bioseqset");
            method = eLoadScope_GBR;
            continue;
        } catch (CException&) {}

        //ERR_POST(Fatal << "Cannot load " << path);
        return eLoadScope_Failed; //should have 'continue'd in one of the tries above
    }

    return method;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class CXcompareAnnotsApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_ProcessComparison(
            CCompareSeqRegions& comparator,
            TSeqPos enclosing_alignment_length);

    void x_ProcessSeqAlignSetFromFile(string filename);
    void x_ProcessMappingRange(
            string q_id,
            TSeqPos q_start,
            TSeqPos q_stop,
            string t_id,
            TSeqPos t_start,
            TSeqPos t_stop);
    void x_ProcessMappingRanges();

    CRef<CScope> m_scope_q;
    CRef<CScope> m_scope_t;
    CRef<CScope> m_scope_id;
    CRef<CScope> m_scope_for_mapper;
    SAnnotSelector m_sel;

    //for x_ProcessMappingRange
    CRef<CMappingRanges> m_mapping_ranges;

    CSeq_id_Handle m_q_id_prev;
    CSeq_id_Handle m_t_id_prev;

    CRef<CSeq_loc> m_q_loc;
    CRef<CSeq_loc> m_t_loc;

    map<int, map<string, string> > m_id_map; //aln_row (normally only 0 and 1) -> from_id -> to_id

    CArgs m_args;
};




void CXcompareAnnotsApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);


    arg_desc->AddOptionalKey("q",
                             "path",
                             "Query Scope",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("q_serial",
                            "serialformat",
                            "Serial Format",
                            CArgDescriptions::eString,
                            "asn_text");
    arg_desc->SetConstraint("q_serial",
                            (new CArgAllow_Strings())
                                ->Allow("asn_text")
                                ->Allow("asn_bin"));

    arg_desc->AddOptionalKey("t",
                             "path",
                             "Target scope",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("t_serial",
                            "serialformat",
                            "Serial Format",
                            CArgDescriptions::eString,
                            "asn_text");
    arg_desc->SetConstraint("t_serial",
                            (new CArgAllow_Strings())
                                ->Allow("asn_text")
                                ->Allow("asn_bin"));


    arg_desc->AddKey("i",
                     "input",
                     "File containing one of the following: "
                     "\n  - list of paths to asn files of alignments"
                     "\n  - pairs of seq-ids (assuming identity alignment) from query and target scopes"
                     "\n  - 6-column input of seq-id1\\start1\\stop1\\seq-id2\\start2\\stop2 - compare via mapping ranges"
                     "\n  - 3-column input of: seq-id\\tstart\\tstop - compare components to level-0 annots",
                     CArgDescriptions::eInputFile);


    arg_desc->AddDefaultKey("i_container",
                            "asntype",
                            "Top-level ASN.1 container type",
                            CArgDescriptions::eString,
                            "Seq-align-set");
    arg_desc->SetConstraint("i_container",
                             (new CArgAllow_Strings())
                                 ->Allow("Seq-align-set")
                                 ->Allow("Seq-annot")
                                 ->Allow("Seq-align"));

    arg_desc->AddDefaultKey("i_serial",
                            "serialformat",
                            "Serial Format",
                            CArgDescriptions::eString,
                            "asn_text");
    arg_desc->SetConstraint("i_serial",
                            (new CArgAllow_Strings())
                                ->Allow("asn_text")
                                ->Allow("asn_bin"));


    arg_desc->AddOptionalKey("id_map",
                             "file",
                             "Convert seq-ids in alignments. (Format: aln_row\\tfrom_id\\tto_id\\n)",
                             CArgDescriptions::eInputFile);


    arg_desc->AddDefaultKey("depth",
                            "integer",
                            "SAnnotSelector resolve depth",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddFlag("sentinel_seqs",
            "If the query seq in the alignment is not in query scope (e.g. local id),"
            " this will create a Seq-entry with spanning RNA feature and add it to scope "
            " such that the sequence placement can be compared to the annotation");
    arg_desc->AddFlag("sentinel_feats",
                "If bioseq title has 'mRNA' in it, add spanning gene and mrna feats if missing;"
                "if the title contains 'gene', add spanning gene feat if missing");
    arg_desc->AddFlag("spliced",
            "If using spliced alignments, this option must be specified such that"
            "a spliced query location is not collapsed to single range as it would by default");
    arg_desc->AddFlag("adaptive_depth",
            "Use adaptive depth in SAnnotSelector (e.g. when dealing with chromosome locations)"
            "Default is false, because normally we don't want to dig down to find features");

    arg_desc->AddFlag("allow_ID",
            "Use ID when explicitly provided scope is lacking necessary info"
            " (e.g. when remapping contig annots from file with chromosome alignments"
            " we would need to get chromosome seq-map from gb to iterate feats in chrom coords)");


    arg_desc->AddOptionalKey("add_qual",
                             "qualkey",
                             "Add additional named qualifier columns q_qualkey and t_qualkey",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);

    arg_desc->AddOptionalKey("add_dbxref",
                             "dbxrefkey",
                             "Add additional named dbxref columns q_dbxrefkey and t_dbxrefkey",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);


    //TODO: Must fix the code to decide whether to use scopeless_mapper automatically

    arg_desc->AddFlag("range_overlap", "Use overlap by ranges to allow comparison between features that overlap by ranges but not by intervals.");
    arg_desc->AddFlag("reverse", "Swap q and t in inputs");
//    arg_desc->AddFlag("long_loc_label", "Long labels for location instead of id/start/stop/strand");
    arg_desc->AddFlag("trace", "Turn on tracing");
    arg_desc->AddFlag("strict_type", "Match features of the same type only");

    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Cross-compare annots on seqs", false);

    SetupArgDescriptions(arg_desc.release());

    #ifdef _DEBUG
        SetDiagPostLevel(eDiag_Info);
        //SetDiagPostLevel(eDiag_Error);
    #else
        SetDiagPostLevel(eDiag_Warning);
        //SetDiagPostLevel(eDiag_Error);
        //SetDiagPostLevel(eDiag_Info);
    #endif


    m_mapping_ranges.Reset(new CMappingRanges);
/*
    DBAPI_RegisterDriver_CTLIB();
    GenBankReaders_Register_Pubseq2();
    BDB_Register_Cache();
*/
    CPluginManager_DllResolver::EnableGlobally(true); //to allow handling of dlls as specified in conffile
}




void CXcompareAnnotsApplication::x_ProcessMappingRanges()
{
    if(!m_q_id_prev) return;

    if(m_q_loc.IsNull() || m_t_loc.IsNull() || m_q_loc->IsNull() || m_t_loc->IsNull()) {
        return;
    }

    CRef<CSeq_loc> qry_rgn_loc = sequence::Seq_loc_Merge(*m_q_loc, CSeq_loc::fMerge_SingleRange, m_scope_q);


    CRef<CSeq_loc_Mapper> simple_mapper(new CSeq_loc_Mapper(*m_q_loc, *m_t_loc, m_scope_for_mapper));
        //when remapping Gnomon ent's loaded from files, must use NULL to remap (or else fails)
        //using m_scope_qt or m_scope_q or m_scqpe_t instead of NULL causes remapping to fail

#if _DEBUG
    string s ="";
    qry_rgn_loc->GetLabel(&s);
    _TRACE("Mapping range q: " + s);

    s = "";
    CRef<CSeq_loc> mapped = simple_mapper->Map(*m_q_loc);
    mapped->GetLabel(&s);
    _TRACE("Mapping range t:" + s);
#endif

    CRef<ILocMapper> mapper(new CLocMapper_Default(
            *simple_mapper
            , *m_scope_q
            , m_args["spliced"]
            , NULL
            , false));

    CRef<CSeq_loc> rgn_loc = sequence::Seq_loc_Merge(*m_q_loc,  CSeq_loc::fMerge_SingleRange, m_scope_q);
    rgn_loc->SetInt().SetStrand(eNa_strand_plus);

    //if we are working with the same scope and same query and target locs,
    //we are comparing the seq against itself, so compare features on different
    //genes only, and report all matches, not only best B|F|R classes, and
    //report overlaps of same types only
    bool self_comparison =
            sequence::IsSameBioseq(
                sequence::GetId(*m_q_loc, m_scope_q),
                sequence::GetId(*m_t_loc, m_scope_t),
                m_scope_q)
            && m_scope_q.GetPointer() == m_scope_t.GetPointer()
            && m_q_loc->Compare(*m_t_loc) == 0;


    TSeqPos len = sequence::GetLength(*m_q_loc, m_scope_q);
    CRef<CCompareSeqRegions> region_comparator(new CCompareSeqRegions(
            *rgn_loc,
            m_scope_q,
            m_scope_t,
            *mapper,
            m_sel,
            m_sel,
            *m_t_id_prev.GetSeqId()));
    if(self_comparison)
    {
        region_comparator->SetOptions() |= CCompareSeqRegions::fDifferentGenesOnly;
        region_comparator->SetOptions() &= ~CCompareSeqRegions::fSelectBest;
        region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
    }

    if(m_args["strict_type"]) region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
    x_ProcessComparison(*region_comparator, len);

    rgn_loc->SetInt().SetStrand(eNa_strand_minus);
    region_comparator.Reset(new CCompareSeqRegions(
                *rgn_loc,
                m_scope_q,
                m_scope_t,
                *mapper,
                m_sel,
                m_sel,
                *m_t_id_prev.GetSeqId()));
    if(self_comparison)
    {
        region_comparator->SetOptions() |= CCompareSeqRegions::fDifferentGenesOnly;
        region_comparator->SetOptions() &= ~CCompareSeqRegions::fSelectBest;
        region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
    }
    if(m_args["strict_type"]) region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
    x_ProcessComparison(*region_comparator, len);
}


void CXcompareAnnotsApplication::x_ProcessMappingRange(
        string str_q_id,
        TSeqPos q_start,
        TSeqPos q_stop,
        string str_t_id,
        TSeqPos t_start,
        TSeqPos t_stop)
{
    CRef<CSeq_id> q_seq_id(new CSeq_id(str_q_id));
    CRef<CSeq_id> t_seq_id(new CSeq_id(str_t_id));
    CSeq_id_Handle q_id = sequence::GetId(*q_seq_id, *m_scope_q);
    CSeq_id_Handle t_id = sequence::GetId(*t_seq_id, *m_scope_t);

    //encountered hit on another q/t pair
    if(m_t_id_prev && (q_id != m_q_id_prev || t_id != m_t_id_prev))
    {
        //process the alignment and reset it
        this->x_ProcessMappingRanges();

        //m_mapping_ranges.Reset(new CMappingRanges);
        m_q_loc.Reset(new CSeq_loc); m_q_loc->SetNull();
        m_t_loc.Reset(new CSeq_loc); m_t_loc->SetNull();

    }

    //add hit to the mapping ranges


    ENa_strand t_strand = eNa_strand_plus;

    if(t_start > t_stop) {
        TSeqPos t = t_start;
        t_start = t_stop;
        t_stop = t;
        t_strand = eNa_strand_minus;
    }

    CRef<CSeq_loc> qloc(new CSeq_loc);
    CRef<CSeq_loc> tloc(new CSeq_loc);

    qloc->SetInt().SetId(*q_seq_id);
    qloc->SetInt().SetFrom(q_start);
    qloc->SetInt().SetTo(q_stop);
    qloc->SetInt().SetStrand(eNa_strand_plus);


    tloc->SetInt().SetId(*t_seq_id);
    tloc->SetInt().SetFrom(t_start);
    tloc->SetInt().SetTo(t_stop);
    tloc->SetInt().SetStrand(t_strand);


    if(m_q_loc.IsNull() || m_q_loc->IsNull()) {
        m_q_loc = qloc;
    } else {
        m_q_loc->Add(*qloc);
    }

    if(m_t_loc.IsNull() || m_t_loc->IsNull()) {
        m_t_loc = tloc;
    } else {
        m_t_loc->Add(*tloc);
    }


    m_t_id_prev = t_id;
    m_q_id_prev = q_id;
}





void CXcompareAnnotsApplication::x_ProcessSeqAlignSetFromFile(string filename)
{
    typedef list<CRef<CSeq_align> > TSeqAlignList;
    typedef map<string, CRef<CAlnMix> > TAlnMixes;

    AutoPtr<CObjectIStream> istr;
    istr.reset(CObjectIStream::Open(StringToSerialFormat(m_args["i_serial"].AsString()), filename));

    CRef<CSeq_align_set> aligns_set;
    CRef<CSeq_annot> aligns_annot;
    TSeqAlignList aligns_list;


    _TRACE("Processing " +  filename);


    while(!istr->EndOfData()) {
        try {
            typedef list<CRef<CSeq_align> > TSeqAlignList;
            if(m_args["i_container"].AsString() == "Seq-align-set") {
                aligns_set.Reset(new CSeq_align_set);
                *istr >> *aligns_set;
            } else if(m_args["i_container"].AsString() == "Seq-annot") {
                aligns_annot.Reset(new CSeq_annot);
                *istr >> *aligns_annot;
            } else if (m_args["i_container"].AsString() == "Seq-align") {
                CRef<CSeq_align> aln(new CSeq_align);
                *istr >> *aln;
                aligns_list.clear();
                aligns_list.push_back(aln);
            } else {
                ERR_POST(Fatal << "Don't know about this format: " << m_args["i_container"].AsString());
            }


            /*
             * We could have stuck all alignments in one CAlnMix, but the resulting
             * merged Seq-align becomes too large and too sparse, (and thus too slow) so it is more efficient to
             * have a CAlnMix for each target sequence
             */


            TAlnMixes alnMixes;



            NON_CONST_ITERATE(TSeqAlignList, it,
                    aligns_set ? aligns_set->Set() :
                    aligns_annot ? aligns_annot->SetData().SetAlign() :
                    aligns_list)
            {


                if(!m_id_map.empty()) {
                    for(int i = 0; i <= m_id_map.rbegin()->first; i++) {
                        CRef<CSeq_id>& seq_id = (*it)->SetSegs().SetDenseg().SetIds()[i];
                        if(seq_id.IsNull()) continue;

                        string str_seq_id = "";
                        seq_id->GetLabel(&str_seq_id);
                        if(m_id_map[i].find(str_seq_id) != m_id_map[i].end()) {
                            seq_id->Set(m_id_map[i][str_seq_id]);
                        }
                    }
                }



                if(m_args["reverse"]) {
                    (*it)->SwapRows(0, 1);
                }



                const CSeq_id& id_q = (*it)->GetSeq_id(0);
                const CSeq_id& id_t = (*it)->GetSeq_id(1);



                string str_id_q = "";
                id_q.GetLabel(&str_id_q);

                string str_id_t = "";
                id_t.GetLabel(&str_id_t);

                string str_aln = "aln:" + str_id_q + "->" + str_id_t;


                if(m_args["sentinel_seqs"] && !m_scope_q->GetBioseqHandle(id_q)) {
                    AddSentinelRNASeq(*m_scope_q, id_q); //for remapper
                }

                if(!IsInScope(*m_scope_q, id_q)) {
                    _TRACE(str_aln +" : query seq not in scope_q");
                    continue;
                } else if(!IsInScope(*m_scope_t, id_t)) {
                    _TRACE(str_aln + " : target seq not in scope_t");
                    continue;
                } else {
                    LOG_POST("Loading " + str_aln);
                    if(alnMixes[str_aln].IsNull()) alnMixes[str_aln].Reset(new CAlnMix(/**m_scope_qt, 0*/));
                    alnMixes[str_aln]->Add(**it, CAlnMix::fPreserveRows /*, CAlnMix::fCalcScore*/); //keep query as row 0
                }
            }


            ITERATE(TAlnMixes, it2, alnMixes) {
                string str_id = it2->first;



                CRef<CAlnMix> aln_mix = it2->second;

                aln_mix->Merge(
                    // CAlnMix::fTruncateOverlaps
                        CAlnMix::fGapJoin
                      | CAlnMix::fMinGap
                    //  | CAlnMix::fSortInputByScore

                    //| CAlnMix::fFillUnalignedRegions
                    //| CAlnMix::fAllowTranslocation
                    );



                CConstRef<CSeq_align> merged_aln(&aln_mix->GetSeqAlign());


                CSeq_align_Base::TDim aln_dim = merged_aln->GetDim();


                //NcbiCout << MSerial_AsnText << *merged_aln;
                //ERR_POST(Fatal << "temp exit");

                //when we have a seq-align of self-to-self, the CAlnMix merges
                //query and target together into a single row. This is unusable for remapping;
                //In this case revert to the original
                _TRACE("Alignment dim: " + NStr::IntToString(merged_aln->GetDim()));
                if(aln_dim == 1) {
                    if(aln_mix->GetInputSeqAligns().size() != 1) {
                        NCBI_THROW(CException, eUnknown, "Multiple alignments collapsed to CAlnMix of dim 1");
                    } else {
                        merged_aln.Reset(aln_mix->GetInputSeqAligns()[0]);
                        aln_dim = 2; //a dangerous assumption
                    }
                }

                // Construct the source sequence loc
                // We use a "source loc" instead of 'whole'
                // beacuse we want to limit the scope of comparison
                // to the region spanned by the alignment
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->SetInt().SetFrom(merged_aln->GetSeqStart(0));
                loc->SetInt().SetTo(merged_aln->GetSeqStop(0));
                //loc->SetInt().SetStrand(eNa_strand_both);

                CRef<CSeq_id> id(new CSeq_id);
                id->Assign(merged_aln->GetSeq_id(0));



                TSeqPos source_len = merged_aln->GetSeqRange(0).GetLength();


                loc->SetInt().SetId(*id);

                for(CSeq_align_Base::TDim row = 1; row < aln_dim; ++row) {

                    CRef<ILocMapper> loc_mapper;

                    //use m_scope_id instead of m_scope_q to create an aligner
                    //because Local Data Storage source have seq-ids without versions, while the alignments are
                    //with versions (i.e. can't remap via versioned alignments from versionless
                    //location from local storage). Need to think through whether this is a legit thing to do
                    //A possible failure scenario is an alignment from an old build with
                    //old contig versions; we process a seq from SplignLDS source
                    //without a version, so a scope gets the latest one and naturally can't remap it
                    //if target scope is loaded from local starage, tell the mapper to strip versions from mapped locations


                    CRef<CSeq_loc_Mapper> simple_mapper(new CSeq_loc_Mapper(
                            *merged_aln,
                            row,
                            m_scope_for_mapper));    //m_scope_qt causes problems resolving seq-id from one scope to another scope





                    loc_mapper.Reset(new CLocMapper_Default(
                            *simple_mapper,
                            *m_scope_q,
                            m_args["spliced"],
                            NULL,
                            false
                    ));



        #if 0 //this doesn't work when mapping a versionless sequence with versioned alignments
          //; the mapper can't map; BUT WHY REMAPPING INDIVIDUAL FEATURES WORKS??

                                //calculate the mapped length
                    CRef<CSeq_loc_Mapper> m2(...);


                    string s = "";
                    CConstRef<CSeq_loc> mapped = m2->Map(*loc);
                    mapped->GetLabel(&s);
                    _TRACE("Enclosing alignment (row " + NStr::IntToString(row)  +") :" + s);

                    TSeqPos enclosing_alignment_length = sequence::GetLength(*mapped, m_scope_t);
        #endif




                    if(m_args["sentinel_feats"]) {
                        AddDefaultSentinelFeats(*m_scope_q, *loc);
                        loc->SetInt().SetStrand(eNa_strand_both);
                    }

                    //Update: now doing it one strand at a time because it is faster
                    //(resolving smaller sized overlap groups)
                    //also, doing it this way we avoid a situation
                    //where non-overlapping features on the same strand
                    //may be put in the same overlap group due to
                    //common overlapping feature on the other strand

                    loc->SetInt().SetStrand(eNa_strand_plus);

                    CRef<CCompareSeqRegions> region_comparator(new CCompareSeqRegions(
                            *loc,
                            m_scope_q,
                            m_scope_t,
                            *loc_mapper,
                            m_sel,
                            m_sel,
                            merged_aln->GetSeq_id(1)));
                    if(m_args["strict_type"]) region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
                    x_ProcessComparison(*region_comparator, source_len);

                    loc->SetInt().SetStrand(eNa_strand_minus);
                    region_comparator.Reset(new CCompareSeqRegions(
                            *loc,
                            m_scope_q,
                            m_scope_t,
                            *loc_mapper,
                            m_sel,
                            m_sel,
                            merged_aln->GetSeq_id(1)));
                    if(m_args["strict_type"]) region_comparator->SetOptions() |= CCompareSeqRegions::fSameTypeOnly;
                    x_ProcessComparison(*region_comparator, source_len);


                } //next row in the merged aln
            } //next aln_mix
        } catch (CException& e) {
            NCBI_REPORT_EXCEPTION("Can't process alignment", e);
        }



        //We want to flush the scope to avoid memory hogging, but not
        //for scopes that were manually loaded (or it will wipe out loaded data)
        m_scope_id->ResetHistory();
        if(!IsManualScope(*m_scope_q)) m_scope_q->ResetHistory();
        if(!IsManualScope(*m_scope_t)) m_scope_t->ResetHistory();
    }//next object from stream
}





void CXcompareAnnotsApplication::x_ProcessComparison(
        CCompareSeqRegions& comparator,
        TSeqPos enclosing_alignment_length)
{
    static unsigned groupNumber = 1;

    string loc_label = "";
    comparator.GetQueryLoc().GetLabel(&loc_label);
    LOG_POST("Processing location " << loc_label);

    //Process comparisons
    vector<CRef<CCompareFeats> > v;

    for(; comparator.NextComparisonGroup(v); groupNumber++) {
        int comparisonNumber = 1;

        _TRACE("Next comparison group");
        ITERATE(vector<CRef<CCompareFeats> >, it, v) {
            _TRACE("Next feat");
            CRef<CCompareFeats> cf = *it;

            if(cf->GetMappedIdentity() <= 0) {continue;}
            if(comparator.GetOptions() & CCompareSeqRegions::fDifferentGenesOnly && cf->GetFeatT().IsNull()) {continue;}



            NcbiCout << groupNumber << "\t" << comparisonNumber << "\t";

            if(!cf->GetFeatQ().IsNull()) {
                _ASSERT(!cf->GetSelfLocQ().IsNull());
                _ASSERT(!cf->GetMappedLocQ().IsNull());

                string s = "";
                feature::GetLabel(*cf->GetFeatQ(), &s, feature::fFGL_Type);
                NcbiCout << s << "\t";

                s = "";
                feature::GetLabel(*cf->GetFeatQ(), &s, feature::fFGL_Content);
                NcbiCout << s << "\t";


                NcbiCout << GetIdStartStopStrandStr(*cf->GetSelfLocQ(), *m_scope_q, false) << "\t";
                NcbiCout << (cf->GetSelfLocQ().IsNull() ? "" : NStr::IntToString(sequence::GetLength(*cf->GetSelfLocQ(), m_scope_q))) << "\t";


                NcbiCout << GetIdStartStopStrandStr(*cf->GetMappedLocQ(), *m_scope_t, false) << "\t";

            } else {
                NcbiCout << "\t\t\t\t\t\t\t\t\t\t\t";
                //if(m_args["long_loc_label"]) NcbiCout << "\t\t";
            }

            if(!cf->GetFeatT().IsNull()) {
                string s = "";
                feature::GetLabel(*cf->GetFeatT(), &s, feature::fFGL_Type);
                NcbiCout << s << "\t";

                s = "";
                feature::GetLabel(*cf->GetFeatT(), &s, feature::fFGL_Content);
                NcbiCout << s << "\t";

                NcbiCout << GetIdStartStopStrandStr(*cf->GetSelfLocT(), *m_scope_t, false) << "\t";
                NcbiCout << (cf->GetSelfLocT().IsNull() ? "" : NStr::IntToString(sequence::GetLength(*cf->GetSelfLocT(), m_scope_t))) << "\t";


            } else {
                NcbiCout << "\t\t\t\t\t\t\t";
                //if(m_args["long_loc_label"]) NcbiCout << "\t";

            }

            if(cf->IsMatch()) {
                NcbiCout.setf(ios::fixed);
                NcbiCout.setf(ios::showpoint);
                NcbiCout.precision(6);

                string sResult = "";
                cf->GetComparison()->GetResult(&sResult);
                NcbiCout << cf->GetComparison()->GetEvidenceString() << "\t";
                cf->GetComparison()->GetResult(&sResult);
                NcbiCout << sResult << "\t";
                NcbiCout << cf->GetMappedIdentity() << "\t";
                NcbiCout << cf->GetComparison()->GetRelativeOverlap() << "\t";
                NcbiCout << cf->GetComparison()->GetSymmetricalOverlap() << "\t";


                float shared_sites_score(0.0f);
                int loc1_intervals(0);
                int loc2_intervals(0);
                cf->GetComparison()->GetSplicingSimilarity(shared_sites_score, &loc1_intervals, &loc2_intervals);
                NcbiCout << loc1_intervals << "\t";
                NcbiCout << loc2_intervals << "\t";
                NcbiCout << shared_sites_score << "\t";

            } else {
                //should have moved mappedIdentity column earlier - hence the awkwardness

                //we don't have Comparison object because there's no comparison, but there was a request to
                //report exon counts anyway, so here we compute them "manually"

                int loc1_intervals(0);
                if(!cf->GetFeatQ().IsNull()) {
                    for (CSeq_loc_CI ci(*cf->GetSelfLocQ()); ci; ++ci) {
                        loc1_intervals++;
                    }
                }


                int loc2_intervals(0);
                if(!cf->GetFeatT().IsNull()) {
                    for (CSeq_loc_CI ci(*cf->GetSelfLocT()); ci; ++ci) {
                        loc2_intervals++;
                    }
                }

                NcbiCout << "\t\t" //no evidence and comment
                         << cf->GetMappedIdentity() << "\t"
                         << "\t\t" //no overlaps
                         <<  (loc1_intervals ? NStr::IntToString(loc1_intervals) : "") << "\t"
                         <<  (loc2_intervals ? NStr::IntToString(loc2_intervals) : "") << "\t"
                         << "\t" //no shared splices score
                         ;
            }



            int qry_gene_id = (cf->GetFeatQ().IsNull() ? 0 : CCompareSeqRegions::s_GetGeneId(*cf->GetFeatQ()));
            int tgt_gene_id = (cf->GetFeatT().IsNull() ? 0 : CCompareSeqRegions::s_GetGeneId(*cf->GetFeatT()));

            NcbiCout
                << enclosing_alignment_length << "\t"
                << (qry_gene_id ? NStr::IntToString(qry_gene_id) : "") << "\t"
                << (tgt_gene_id ? NStr::IntToString(tgt_gene_id) : "") << "\t";


            //report products
            NcbiCout << (!cf->GetFeatQ().IsNull() ? GetProductLabel(*cf->GetFeatQ(), *m_scope_id) : "") << "\t";
            NcbiCout << (!cf->GetFeatT().IsNull() ? GetProductLabel(*cf->GetFeatT(), *m_scope_id) : "") << "\t";

            int ir = cf->GetIrrelevance();
            NcbiCout << (ir == 0 ? "B" : ir == 1 ? "F" : ir == 2 ? "R" : "O");



            //add qual and dbxref values
            if(m_args["add_qual"]) {
                ITERATE(CArgValue::TStringArray, it2, m_args["add_qual"].GetStringList()) {
                    NcbiCout << "\t" << (cf->GetFeatQ().IsNull() ? "" : cf->GetFeatQ()->GetNamedQual(*it2));
                    NcbiCout << "\t" << (cf->GetFeatT().IsNull() ? "" : cf->GetFeatT()->GetNamedQual(*it2));
                }
            }

            if(m_args["add_dbxref"]) {
                ITERATE(CArgValue::TStringArray, it2, m_args["add_dbxref"].GetStringList()) {
                    string str_tag = *it2;
                    for(int j = 0; j < 2; j++) {
                        CConstRef<CSeq_feat> feat = j == 0 ? cf->GetFeatQ() : cf->GetFeatT();
                        //CScope& current_scope     = j == 0 ? *m_scope_q : *m_scope_t;
                        string tmpstr = "";
                        if (!feat.IsNull()) {
                            //special values prefixed with @ are special cases to represent
                            //not dbxrefs per se but some other requested attributes of the feature, e.g.
                            //feat_id or feat_id of the gene xref
                            if(str_tag == "@feat_id" && feat->CanGetId() && feat->GetId().IsLocal()) {
                                tmpstr = NStr::IntToString(feat->GetId().GetLocal().GetId());
                            } else if(str_tag == "@gene_feat_id") {
                                //if feature is a gene - get it from feat-id; otherwise from feat-id of gene xref
                                if(feat->GetData().IsGene() && feat->CanGetId() && feat->GetId().IsLocal()) {
                                    tmpstr = NStr::IntToString(feat->GetId().GetLocal().GetId());
                                } else {
                                    ITERATE(CSeq_feat::TXref, it, feat->GetXref()) {
                                        const CSeqFeatXref& ref = **it;
                                        if (ref.IsSetData() && ref.GetData().IsGene() && ref.CanGetId() && ref.GetId().IsLocal()) {
                                            tmpstr = NStr::IntToString(ref.GetId().GetLocal().GetId());
                                            break;
                                        }
                                    }
                                }
                            } else {
                                CConstRef<CDbtag> db_tag = feat->GetNamedDbxref(str_tag);
                                if(!db_tag.IsNull()) {
                                    db_tag->GetLabel(&tmpstr);
                                }
                            }


                        }
                        NcbiCout << "\t" << tmpstr;
                    }
                }
            }






            NcbiCout << "\n" << flush;
            comparisonNumber++;
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
int CXcompareAnnotsApplication::Run(void)
{
    m_args.Assign(GetArgs());

    string args_str = "";
    m_args.Print(args_str);
    CTime time;
    time.SetCurrent();

    LOG_POST("Starting on " << time.AsString());
    LOG_POST("Args: " << args_str);

    if(m_args["trace"]) {
        SetDiagTrace(eDT_Enable);
        SetDiagPostFlag(eDPF_All);
    }

    m_sel.SetSearchUnresolved(); //need for manually supplied far-reference annots with no seq-entries for scaffolds
    if(m_args["adaptive_depth"]) {
        m_sel.SetAdaptiveDepth();
    } else {
        m_sel.SetExactDepth();
    }
    m_sel.SetResolveAll(); //not sure

    m_sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
    m_sel.IncludeFeatType(CSeqFeatData::e_Rna);
    m_sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    m_sel.IncludeFeatType(CSeqFeatData::e_Gene);
    m_sel.SetResolveDepth(m_args["depth"].AsInteger());

    if(m_args["range_overlap"]) {
        m_sel.SetOverlapTotalRange();
    } else {
        m_sel.SetOverlapIntervals();
    }


    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eNonDefault);

    m_scope_q.Reset(new CScope(*object_manager));
    m_scope_t.Reset(new CScope(*object_manager));




    m_scope_id.Reset(new CScope(*object_manager)); //need this to translate between gi's, accs and accvers
                                                   //as different sources and alignments may be using different types of ids

    m_scope_id->AddDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
    bool use_scopeless_mapper = false;
    if(m_args["q"]) {
        TLoadScopeMethod res = LoadScope(m_args["q"].AsString(), *m_scope_q, StringToSerialFormat(m_args["q_serial"].AsString()));
        if(res == eLoadScope_Failed) {
            ERR_POST(Fatal << "Can't load query scope");
        }
        if(m_args["allow_ID"]) {
            m_scope_q->AddDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
        }

        if(res == eLoadScope_SeqAnnot || res == eLoadScope_SeqEntry) {
            use_scopeless_mapper = true;
        }
    } else {
        m_scope_q = m_scope_id;
    }
    LOG_POST(Info << "Loaded query scope on " << time.SetCurrent().AsString());

    if(m_args["t"] && m_args["q"] && m_args["t"].AsString() == m_args["q"].AsString()) {
        m_scope_t = m_scope_q;
    } else if(m_args["t"]) {
        bool res = LoadScope(m_args["t"].AsString(), *m_scope_t, StringToSerialFormat(m_args["t_serial"].AsString()));
        if(res == eLoadScope_Failed) {
            ERR_POST(Fatal << "Can't load target scope");
        }
        if(m_args["allow_ID"]) {
            m_scope_t->AddDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
        }
    } else {
        m_scope_t = m_scope_id;
    }
    LOG_POST(Info << "Loaded target scope on " << time.SetCurrent().AsString());


    /**************************************************************************
     *
     * Fill seq-id synonyms, if provided
     *
     *************************************************************************/
    if(m_args["id_map"]) {
        LOG_POST("Loading id conversion map");
        CNcbiIstream& istr=m_args["id_map"].AsInputFile();
        string line;
        while(getline(istr, line).good()) {
            if(line.size() == 0 || line.compare(0, 1, "#") == 0) continue;
            vector<string> tokens;
            NStr::Tokenize(line, "\t", tokens);
            if(tokens.size() != 3) {
                ERR_POST(Fatal << "Unexpected input it id_map. Execting 3 columns" << line);
            } else {
                m_id_map[NStr::StringToInt(tokens[0])][tokens[1]] = tokens[2];
            }
        }
    }


    m_scope_q->UpdateAnnotIndex();
    m_scope_t->UpdateAnnotIndex();

    if(use_scopeless_mapper) {
        m_scope_for_mapper.Reset();
    } else {

#if 0
        m_scope_for_mapper = m_scope_q;
#else
        m_scope_for_mapper.Reset(new CScope(*object_manager));
        m_scope_for_mapper->AddScope(*m_scope_q, 1);
            //trying to use priorities because CAlnMix sometimes resolves a seq-id from one scope to
            //a synonym from another scope
        m_scope_for_mapper->AddScope(*m_scope_t, 2);
        m_scope_for_mapper->UpdateAnnotIndex();
#endif
    }

    LOG_POST(Info << "Finished initalizing scopes on " << time.SetCurrent().AsString());



    NcbiCout << "#"
             << "Group\t"
             << "N_in_group\t"
             << "Qry_type\t"
             << "Qry_name\t"
             << "Qry_id\t"
             << "Qry_start\t"
             << "Qry_stop\t"
             << "Qry_strand\t"
             << "Qry_len\t"
             << "Qry_mapped_id\t"
             << "Qry_mapped_start\t"
             << "Qry_mapped_stop\t"
             << "Qry_mapped_strand\t"
             << "Tgt_type\t"
             << "Tgt_name\t"
             << "Tgt_id\t"
             << "Tgt_start\t"
             << "Tgt_stop\t"
             << "Tgt_strand\t"
             << "Tgt_len\t"
             << "Comparison\t"
             << "Comment\t"
             << "Mapped_identity\t"
             << "Relative_overlap\t"
             << "Symmetric_overlap\t"
             << "Qry_exons\t"
             << "Tgt_exons\t"
             << "Splicing_similarity\t"
             << "Aln_q_length\t"
             << "Qry_LocusID\t"
             << "Tgt_LocusID\t"
             << "Qry_product\t"
             << "Tgt_product\t"
             << "Preference";


    //add qual and dbxref headers
    if(m_args["add_qual"]) {
        ITERATE(CArgValue::TStringArray, it, m_args["add_qual"].GetStringList()) {
            NcbiCout << "\tq_" << *it;
            NcbiCout << "\tt_" << *it;
        }
    }

    if(m_args["add_dbxref"]) {
        ITERATE(CArgValue::TStringArray, it, m_args["add_dbxref"].GetStringList()) {
            NcbiCout << "\tq_" << *it;
            NcbiCout << "\tt_" << *it;
        }
    }

    NcbiCout << "\n";






    CNcbiIstream& istr = m_args["i"].AsInputFile();

    string line;
    while (getline(istr, line).good()) {
        if(line.size() == 0 || line.compare(0, 1, "#") == 0) continue;
        vector<string> tokens;
        NStr::Tokenize(line, "\t", tokens);

        if(m_args["i"].AsString().find(".asn") != string::npos && tokens[0].find(":=") != string::npos)
        {
            try {
                this->x_ProcessSeqAlignSetFromFile(m_args["i"].AsString());
            }  catch (CException& e) {
                NCBI_REPORT_EXCEPTION("Cannot process alignment file\n" + line, e);
            }
            break;
        } else if(tokens.size() == 1) {
            try {
                this->x_ProcessSeqAlignSetFromFile(tokens[0]);
             } catch (CException& e) {
                 NCBI_REPORT_EXCEPTION("Cannot process alignment file\n" + line, e);
             }
        } else if(tokens.size() == 6 || tokens.size() == 2) {
            try {
                //either pairs of seq_ids, or pair of mapping ranges (seq_id\tstart\tstop)
                TSeqPos kMaxPos = std::numeric_limits<TSeqPos>::max() - 100;
                    //-100 because there are some special reserved values up there, e.g. kInvalidSeqPos

                string q_id = tokens[0];
                string t_id = tokens.size() == 2 ? tokens[1] : tokens[3];
                TSeqPos q_start = 0;
                TSeqPos q_stop = kMaxPos;
                TSeqPos t_start = 0;
                TSeqPos t_stop = kMaxPos;

                if(tokens.size() == 6) {
                    q_start = NStr::StringToUInt(tokens[1]) - 1;
                    q_stop = NStr::StringToUInt(tokens[2]) - 1;
                    t_start = NStr::StringToUInt(tokens[4]) - 1;
                    t_stop = NStr::StringToUInt(tokens[5]) - 1;
                }

                if(m_args["reverse"]) {
                    std::swap(t_id, q_id);
                    std::swap(q_start, t_start);
                    std::swap(q_stop, t_stop);
                }

                this->x_ProcessMappingRange(
                        q_id,
                        q_start,
                        q_stop,
                        t_id,
                        t_start,
                        t_stop);
            } catch(CException& e) {
                NCBI_REPORT_EXCEPTION("Cannot process mapping ranges at line\n" + line, e);

                m_q_loc.Reset(new CSeq_loc); m_q_loc->SetNull();
                m_t_loc.Reset(new CSeq_loc); m_t_loc->SetNull();
            }

        } else if(tokens.size() == 3) {
            try {
                CRef<CSeq_loc> loc(new CSeq_loc);
                CRef<CSeq_id> id(new CSeq_id(tokens[0]));
                if(tokens[1] == "" || tokens[2] == "") {
                    loc->SetWhole(*id);
                } else {
                    loc->SetInt().SetId(*id);
                    loc->SetInt().SetFrom(NStr::StringToUInt(tokens[1]) - 1);
                    loc->SetInt().SetTo(NStr::StringToUInt(tokens[2]) - 1);
                }


                SAnnotSelector sel;
                sel.SetResolveAll();
                sel.SetExactDepth(true);
                sel.SetAdaptiveDepth(false);
                sel.SetResolveDepth(0);

                sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
                sel.IncludeFeatType(CSeqFeatData::e_Rna);
                sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
                sel.IncludeFeatType(CSeqFeatData::e_Gene);

                CRef<CSeq_loc_Mapper> simple_mapper(new CSeq_loc_Mapper(
                        m_scope_q->GetBioseqHandle(*id),
                        CSeq_loc_Mapper::eSeqMap_Up));

                CRef<ILocMapper> mapper(new CLocMapper_Default(
                        *simple_mapper
                        , *m_scope_q
                        , m_args["spliced"]
                        , NULL
                        , false));

#if 1
                //need to process seq-map manually instead of just having
                //SAnnotSelector iterate on lower depth, because if we are to use
                //GetMappedFeature, we lose information about locations on components,
                //as the mapped feature is remapped to level 0.
                //If we use GetOriginalFeature, some locations on components may be
                //in terms of lower-level components, and self-mappers will not be able
                //to process those (they will be stripped)

                SSeqMapSelector sel_map;
                for(CSeqMap_CI ci(m_scope_q->GetBioseqHandle(*id), sel_map); ci; ++ci) {
                    if(ci.GetType() != CSeqMap::eSeqRef) {
                        continue;
                    }

                    CRef<CSeq_loc> loc_q(new CSeq_loc);
                    CRef<CSeq_id> id_q(new CSeq_id);
                    id_q->Assign(*ci.GetRefSeqid().GetSeqId());
                    loc_q->SetInt().SetId(*id_q);
                    loc_q->SetInt().SetFrom(ci.GetRefPosition());
                    loc_q->SetInt().SetTo(ci.GetEndPosition());
                    //loc_q->SetInt().SetStrand(ci.GetRefMinusStrand() ? eNa_strand_minus : eNa_strand_plus);
                    loc_q->SetInt().SetStrand(eNa_strand_both);

                    CRef<CCompareSeqRegions> region_comparator(new CCompareSeqRegions(
                            *loc_q,
                            m_scope_q,
                            m_scope_t,
                            *mapper,
                            sel,
                            sel,
                            *id));

                    x_ProcessComparison(*region_comparator, 0);

                }
#endif




            } catch(CException& e) {
                NCBI_REPORT_EXCEPTION("Cannot process self-comparison at line\n" + line, e);
            }
        } else {
            ERR_POST(Fatal << "Unexpected number of columns, " << tokens.size() << line);
        }

        //Resetting history after every processing seems to reduce
        //memory gobbling. Must not do that to manually loaded scopes as
        //it will unload them
        m_scope_id->ResetHistory();
        if(!IsManualScope(*m_scope_q)) m_scope_q->ResetHistory();
        if(!IsManualScope(*m_scope_t)) m_scope_t->ResetHistory();
    }

    //process alignments accumulated by x_ProcessMappingRange, if any, one last time
    this->x_ProcessMappingRanges();

    LOG_POST(Info << "Done on " << time.SetCurrent().AsString());
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CXcompareAnnotsApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
