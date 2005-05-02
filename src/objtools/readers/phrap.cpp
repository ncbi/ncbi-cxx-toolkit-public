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
* Authors:  Aleksey Grichenko, NCBI.
*
* File Description:
*   Reader for Phrap-format files.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/phrap.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Read whole line from a stream
inline
string ReadLine(CNcbiIstream& in)
{
    in >> ws;
    string ret;
    char buf[1024];
    do {
        in.clear();
        in.getline(buf, 1024);
        ret += string(buf);
    } while ( !in.eof()  &&  in.fail() );
    if ( in.fail() ) {
        in.clear();
    }
    return ret;
}


inline
void CheckStreamState(CNcbiIstream& in,
                        string err_msg)
{
    if ( in.fail() ) {
        in.clear(); // to get correct position
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: failed to read " + err_msg,
                    in.tellg() - CT_POS_TYPE(0));
    }
}


class CPhrap_Seq : public CObject
{
public:
    CPhrap_Seq(TPhrapReaderFlags flags);
    void Read(CNcbiIstream& in);
    void ReadData(CNcbiIstream& in);
    virtual void ReadTag(CNcbiIstream& in, char tag) = 0;

    void SetComplemented(bool value) { m_Complemented = value; }
    bool IsComplemented(void) const
        {
            return m_Complemented  &&  !FlagSet(fPhrap_NoComplement);
        }

    // Pad position is 0-based number indicating where to insert the pad.
    // E.g.:
    // unpadded pos:    0 1 2 3 4 5 6 7 8 9
    // padded pos:      0 1 - 2 3 - 4 - - 5
    // pad value:           2     4   5 5
    // sequence:        a a * a a * a * * a

    TPhrapReaderFlags GetFlags(void) const { return m_Flags; }
    bool FlagSet(EPhrapReaderFlags value) const
        { return (m_Flags & value) != 0; }

    const string& GetName(void) const { return m_Name; }
    TSeqPos GetPaddedLength(void) const { return m_PaddedLength; }
    TSeqPos GetUnpaddedLength(void) const { return m_UnpaddedLength; }
    const string& GetData(void) const { return m_Data; }
    CRef<CSeq_id> GetId(void) const;

    TSeqPos GetPaddedPos(TSeqPos unpadded) const;
    TSeqPos GetUnpaddedPos(TSeqPos padded,
                           TSeqPos* link = 0) const;

    CRef<CBioseq> CreateBioseq(void) const;

    typedef map<TSeqPos, TSeqPos> TPadMap;
    const TPadMap& GetPadMap(void) const { return m_PadMap; }

protected:
    void CreatePadsFeat(CRef<CSeq_annot>& annot) const;
    void CopyFrom(const CPhrap_Seq& seq);

private:
    void x_FillSeqData(CSeq_data& data) const;

    TPhrapReaderFlags m_Flags;

    string          m_Name;
    TSeqPos         m_PaddedLength;
    TSeqPos         m_UnpaddedLength;
    string          m_Data;   // pads are already removed
    TPadMap         m_PadMap; // shifts for unpadded positions
    bool            m_Complemented;

    mutable CRef<CSeq_id> m_Id;
};


const char kPadChar = '*';

CPhrap_Seq::CPhrap_Seq(TPhrapReaderFlags flags)
    : m_Flags(flags),
      m_PaddedLength(0),
      m_UnpaddedLength(0),
      m_Complemented(false)
{
}


void CPhrap_Seq::CopyFrom(const CPhrap_Seq& seq)
{
    m_Name = seq.m_Name;
    m_PaddedLength = seq.m_PaddedLength;
}


void CPhrap_Seq::Read(CNcbiIstream& in)
{
    if ( !m_Name.empty() ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: redefinition of " + m_Name + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    in >> m_Name >> m_PaddedLength;
    CheckStreamState(in, "sequence header.");
}


void CPhrap_Seq::ReadData(CNcbiIstream& in)
{
    _ASSERT(m_Data.empty());
    string line;
    TSeqPos cnt = 0;
    while (!in.eof()  &&  cnt < m_PaddedLength) {
        in >> line;
        m_Data += NStr::ToUpper(line);
        cnt += line.size();
    }
    char next = in.eof() ? ' ' : in.peek();
    if ( m_Data.size() != m_PaddedLength  || !isspace(next) ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: invalid data length for " + m_Name + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    size_t new_pos = 0;
    for (size_t pos = 0; pos < m_PaddedLength; pos++) {
        if (m_Data[pos] == kPadChar) {
            m_PadMap[pos] = pos - new_pos;
            continue;
        }
        m_Data[new_pos] = m_Data[pos];
        new_pos++;
    }
    m_UnpaddedLength = new_pos;
    m_Data.resize(m_UnpaddedLength);
    m_PadMap[m_PaddedLength] = m_PaddedLength - m_UnpaddedLength;
}


inline
TSeqPos CPhrap_Seq::GetPaddedPos(TSeqPos unpadded) const
{
    TPadMap::const_iterator pad = m_PadMap.lower_bound(unpadded);
    while (unpadded <= pad->first - pad->second) {
        pad++;
        _ASSERT(pad != m_PadMap.end());
    }
    return unpadded + pad->second;
}


inline
TSeqPos CPhrap_Seq::GetUnpaddedPos(TSeqPos padded,
                                   TSeqPos* link) const
{
    TPadMap::const_iterator pad = m_PadMap.lower_bound(padded);
    while (pad != m_PadMap.end()  &&  pad->first == padded) {
        ++padded;
        ++pad;
        if (link) {
            ++(*link);
        }
    }
    if (pad == m_PadMap.end()) {
        return kInvalidSeqPos;
    }
    return padded - pad->second;
}


inline
CRef<CSeq_id> CPhrap_Seq::GetId(void) const
{
    if (!m_Id) {
        m_Id.Reset(new CSeq_id);
        m_Id->SetLocal().SetStr(m_Name);
    }
    return m_Id;
}


CRef<CBioseq> CPhrap_Seq::CreateBioseq(void) const
{
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetId());
    CSeq_inst& inst = seq->SetInst();
    inst.SetMol(CSeq_inst::eMol_dna);                                          // ???
    inst.SetLength(m_UnpaddedLength);

    // char case, pads, coding ???
    x_FillSeqData(inst.SetSeq_data());

    return seq;
}


void CPhrap_Seq::x_FillSeqData(CSeq_data& data) const
{
    data.SetIupacna().Set(m_Data);
    if ( IsComplemented() ) {
        CSeqportUtil::ReverseComplement(&data, 0, m_UnpaddedLength);
    }
    if ( FlagSet(fPhrap_PackSeqData) ) {
        CSeqportUtil::Pack(&data);
    }
}


void CPhrap_Seq::CreatePadsFeat(CRef<CSeq_annot>& annot) const
{
    // One pad is artificial and indicates end of sequence
    if ( !FlagSet(fPhrap_FeatGaps)  ||  m_PadMap.size() <= 1 ) {
        return;
    }
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetData().SetImp().SetKey("gap_set");                                // ???
    feat->SetComment("Gap set for " + m_Name);                                 // ???
    CPacked_seqpnt& pnts = feat->SetLocation().SetPacked_pnt();
    pnts.SetId(*GetId());

    size_t num_gaps = m_PadMap.size() - 1;
    pnts.SetPoints().resize(num_gaps);
    size_t i = 0;
    ITERATE(TPadMap, pad_it, m_PadMap) {
        if ( pad_it->first >= GetPaddedLength() ) {
            // Skip the last artficial pad
            break;
        }
        TSeqPos pos = pad_it->first - pad_it->second;
        if ( IsComplemented() ) {
            pnts.SetPoints()[num_gaps - i - 1] =
                GetUnpaddedLength() - pos;
        }
        else {
            pnts.SetPoints()[i] = pos;
        }
        i++;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    annot->SetData().SetFtable().push_back(feat);
}


class CPhrap_Read : public CPhrap_Seq
{
public:
    typedef map<string, CRef<CPhrap_Read> > TReads;

    CPhrap_Read(TPhrapReaderFlags flags);
    ~CPhrap_Read(void);

    void Read(CNcbiIstream& in);

    struct SReadDS
    {
        string m_ChromatFile;
        string m_PhdFile;
        string m_Time;
        string m_Chem;
        string m_Dye;
        string m_Template;
        string m_Direction;
    };

    struct SReadTag
    {
        string  m_Type;
        string  m_Program;
        TSeqPos m_Start;
        TSeqPos m_End;
        string  m_Date;
    };
    typedef vector<SReadTag> TReadTags;
    typedef vector<TSignedSeqPos> TStarts;
    typedef CRange<TSignedSeqPos> TRange;

    void AddReadLoc(TSignedSeqPos start, bool complemented);

    const TStarts& GetStarts(void) const { return m_Starts; }

    void ReadQuality(CNcbiIstream& in);  // QA
    void ReadDS(CNcbiIstream& in);       // DS
    virtual void ReadTag(CNcbiIstream& in, char tag); // RT{}

    CRef<CSeq_entry> CreateRead(void) const;

    void CopyFrom(const CPhrap_Read& read);

private:
    void x_CreateFeat(CBioseq& bioseq) const;
    void x_CreateDesc(CBioseq& bioseq) const;
    void x_AddTagFeats(CRef<CSeq_annot>& annot) const;
    void x_AddQualityFeat(CRef<CSeq_annot>& annot) const;

    size_t    m_NumInfoItems;
    size_t    m_NumReadTags;
    TRange    m_HiQualRange;
    TRange    m_AlignedRange;
    TStarts   m_Starts;
    SReadDS*  m_DS;
    TReadTags m_Tags;
};


CPhrap_Read::CPhrap_Read(TPhrapReaderFlags flags)
    : CPhrap_Seq(flags),
      m_NumInfoItems(0),
      m_NumReadTags(0),
      m_HiQualRange(TRange::GetEmpty()),
      m_AlignedRange(TRange::GetEmpty()),
      m_DS(0)
{
}


CPhrap_Read::~CPhrap_Read(void)
{
    if ( m_DS ) {
        delete m_DS;
    }
}


void CPhrap_Read::CopyFrom(const CPhrap_Read& read)
{
    CPhrap_Seq::CopyFrom(read);
    m_NumInfoItems = read.m_NumInfoItems;
    m_NumReadTags = read.m_NumReadTags;
}


void CPhrap_Read::Read(CNcbiIstream& in)
{
    CPhrap_Seq::Read(in);
    in >> m_NumInfoItems >> m_NumReadTags;
    CheckStreamState(in, "RD data.");
}


void CPhrap_Read::ReadQuality(CNcbiIstream& in)
{
    TSignedSeqPos start, stop;
    in >> start >> stop;
    CheckStreamState(in, "QA data.");
    if (start > 0  &&  stop > 0) {
        m_HiQualRange.Set(start - 1, stop - 1);
    }
    in >> start >> stop;
    CheckStreamState(in, "QA data.");
    if (start > 0  &&  stop > 0) {
        m_AlignedRange.Set(start - 1, stop - 1);
    }
}


void CPhrap_Read::ReadDS(CNcbiIstream& in)
{
    if ( m_DS ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: DS redifinition for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    m_DS = new SReadDS;
    string tag = ReadLine(in);
    list<string> values;
    NStr::Split(tag, " ", values, NStr::eNoMergeDelims);
    bool in_time = false;
    ITERATE(list<string>, it, values) {
        if (*it == "CHROMAT_FILE:") {
            m_DS->m_ChromatFile = *(++it);
        }
        else if (*it == "PHD_FILE:") {
            m_DS->m_PhdFile = *(++it);
        }
        else if (*it == "CHEM:") {
            m_DS->m_Chem = *(++it);
        }
        else if (*it == "DYE:") {
            m_DS->m_Dye = *(++it);
        }
        else if (*it == "TEMPLATE:") {
            m_DS->m_Template = *(++it);
        }
        else if (*it == "DIRECTION:") {
            m_DS->m_Direction = *(++it);
        }
        else if (*it == "TIME:") {
            in_time = true;
            m_DS->m_Time = *(++it);
            continue;
        }
        else {
            if ( in_time ) {
                m_DS->m_Time += " " + *it;
                continue;
            }
            // _ASSERT("unknown value", 0);
        }
        in_time = false;
    }
}


void CPhrap_Read::ReadTag(CNcbiIstream& in, char tag)
{
    _ASSERT(tag == 'R');
    SReadTag rt;
    in  >> rt.m_Type
        >> rt.m_Program
        >> rt.m_Start
        >> rt.m_End
        >> rt.m_Date
        >> ws; // skip spaces
    CheckStreamState(in, "RT{} data.");
    if (in.get() != '}') {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: '}' expected after RT tag",
                    in.tellg() - CT_POS_TYPE(0));
    }
    if (rt.m_Start > 0) {
        rt.m_Start--;
    }
    if( rt.m_End > 0) {
        rt.m_End--;
    }
    m_Tags.push_back(rt);
}


inline
void CPhrap_Read::AddReadLoc(TSignedSeqPos start, bool complemented)
{
    SetComplemented(complemented);
    m_Starts.push_back(start);
}


void CPhrap_Read::x_AddTagFeats(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatTags)  ||  m_Tags.empty() ) {
        return;
    }
    if (m_Tags.size() != m_NumReadTags) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: invalid number of RT tags for " + GetName() + ".",
                    CT_POS_TYPE(-1));
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    ITERATE(TReadTags, tag_it, m_Tags) {
        const SReadTag& tag = *tag_it;
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetTitle("created " + tag.m_Date + " by " + tag.m_Program);
        feat->SetData().SetImp().SetKey(tag.m_Type);                           // ???
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() -
                GetUnpaddedPos(tag.m_End) - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() -
                GetUnpaddedPos(tag.m_Start) - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
        }
        else {
            loc.SetInt().SetFrom(GetUnpaddedPos(tag.m_Start));
            loc.SetInt().SetTo(GetUnpaddedPos(tag.m_End));
        }
        annot->SetData().SetFtable().push_back(feat);
    }
}


void CPhrap_Read::x_AddQualityFeat(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatQuality)  ||
        (m_HiQualRange.Empty()  &&  m_AlignedRange.Empty())) {
        return;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    if ( !m_HiQualRange.Empty() ) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetImp().SetKey("high_quality_segment");               // ???
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        TSeqPos start = GetUnpaddedPos(m_HiQualRange.GetFrom());
        TSeqPos stop = GetUnpaddedPos(m_HiQualRange.GetTo());
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() - stop - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() - start - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
        }
        else {
            loc.SetInt().SetFrom(start);
            loc.SetInt().SetTo(stop);
        }
        annot->SetData().SetFtable().push_back(feat);
    }
    if ( !m_AlignedRange.Empty() ) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetImp().SetKey("aligned_segment");                    // ???
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        TSeqPos start = GetUnpaddedPos(m_AlignedRange.GetFrom());
        TSeqPos stop = GetUnpaddedPos(m_AlignedRange.GetTo());
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() - stop - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() - start - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
        }
        else {
            loc.SetInt().SetFrom(start);
            loc.SetInt().SetTo(stop);
        }
        annot->SetData().SetFtable().push_back(feat);
    }
}


void CPhrap_Read::x_CreateFeat(CBioseq& bioseq) const
{
    CRef<CSeq_annot> annot;
    CreatePadsFeat(annot);
    x_AddTagFeats(annot);
    x_AddQualityFeat(annot);
    if ( annot ) {
        bioseq.SetAnnot().push_back(annot);
    }
}


void CPhrap_Read::x_CreateDesc(CBioseq& bioseq) const
{
    if ( !FlagSet(fPhrap_Descr)  ||  !m_DS ) {
        return;
    }
    CRef<CSeq_descr> descr(new CSeq_descr);
    CRef<CSeqdesc> desc;

    if ( !m_DS->m_ChromatFile.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("CHROMAT_FILE: " + m_DS->m_ChromatFile);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_PhdFile.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("PHD_FILE: " + m_DS->m_PhdFile);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_Chem.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("CHEM: " + m_DS->m_Chem);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_Direction.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("DIRECTION: " + m_DS->m_Direction);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_Dye.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("DYE: " + m_DS->m_Dye);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_Template.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetComment("TEMPLATE: " + m_DS->m_Template);
        descr->Set().push_back(desc);
    }
    if ( !m_DS->m_Time.empty() ) {
        desc.Reset(new CSeqdesc);
        desc->SetCreate_date().SetStr(m_DS->m_Time);
        descr->Set().push_back(desc);
    }
    bioseq.SetDescr(*descr);
}


CRef<CSeq_entry> CPhrap_Read::CreateRead(void) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CRef<CBioseq> bioseq = CreateBioseq();
    _ASSERT(bioseq);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);                                        // ???

    x_CreateDesc(*bioseq);
    x_CreateFeat(*bioseq);

    entry->SetSeq(*bioseq);
    // Base qualities???
    return entry;
}


class CPhrap_Contig : public CPhrap_Seq
{
public:
    CPhrap_Contig(TPhrapReaderFlags flags);
    void Read(CNcbiIstream& in);

    struct SBaseSeg
    {
        TSeqPos m_Start;         // padded start consensus position
        TSeqPos m_End;           // padded end consensus position
    };

    struct SOligo
    {
        string m_Name;
        string m_Data;
        string m_MeltTemp;
        bool   m_Complemented;
    };
    struct SContigTag
    {
        string          m_Type;
        string          m_Program;
        TSeqPos         m_Start;
        TSeqPos         m_End;
        string          m_Date;
        bool            m_NoTrans;
        vector<string>  m_Comments;
        SOligo          m_Oligo;
    };

    typedef vector<int>            TBaseQuals;
    typedef vector<SBaseSeg>       TBaseSegs;
    typedef map<string, TBaseSegs> TBaseSegMap;
    typedef vector<SContigTag>     TContigTags;
    typedef CPhrap_Read::TReads    TReads;

    const TBaseQuals& GetBaseQualities(void) const { return m_BaseQuals; }

    void ReadBaseQualities(CNcbiIstream& in); // BQ
    void ReadReadLocation(CNcbiIstream& in);  // AF
    void ReadBaseSegment(CNcbiIstream& in);   // BS
    virtual void ReadTag(CNcbiIstream& in, char tag); // CT{}

    CRef<CSeq_entry> CreateContig(int level) const;

    CRef<CPhrap_Read>& SetRead(const CPhrap_Read& read);

private:
    void x_CreateGraph(CBioseq& bioseq) const;
    void x_CreateAlign(CBioseq& bioseq) const;
    void x_CreateFeat(CBioseq& bioseq) const;
    void x_CreateDesc(CBioseq& bioseq) const;

    void x_AddBaseSegFeats(CRef<CSeq_annot>& annot) const;
    void x_AddReadLocFeats(CRef<CSeq_annot>& annot) const;
    void x_AddTagFeats(CRef<CSeq_annot>& annot) const;

    void x_CreateAlignPairs(CBioseq& bioseq) const;
    void x_CreateAlignAll(CBioseq& bioseq) const;
    void x_CreateAlignOptimized(CBioseq& bioseq) const;

    struct SAlignInfo {
        typedef CRange<TSeqPos> TRange;

        SAlignInfo(size_t idx) : m_SeqIndex(idx) {}

        size_t m_SeqIndex;  // index of read (>0) or contig (0)
        TSeqPos m_Start;    // ungapped aligned start
    };
    typedef CRangeMultimap<SAlignInfo, TSeqPos> TAlignMap;
    typedef set<TSeqPos>                        TAlignStarts;
    typedef vector< CConstRef<CPhrap_Seq> >     TAlignRows;

    bool x_AddAlignRanges(TSeqPos           global_start,
                          TSeqPos           global_stop,
                          const CPhrap_Seq& seq,
                          size_t            seq_idx,
                          TSignedSeqPos     offset,
                          TAlignMap&        aln_map,
                          TAlignStarts&     aln_starts) const;
    CRef<CSeq_align> x_CreateSeq_align(TAlignMap&     aln_map,
                                       TAlignStarts&  aln_starts,
                                       TAlignRows&    rows) const;


    size_t            m_NumReads;
    size_t            m_NumSegs;
    TBaseQuals        m_BaseQuals;
    TBaseSegMap       m_BaseSegMap;
    TContigTags       m_Tags;
    mutable TReads    m_Reads;
    bool              m_Circular;
};


CPhrap_Contig::CPhrap_Contig(TPhrapReaderFlags flags)
    : CPhrap_Seq(flags),
      m_NumReads(0),
      m_NumSegs(0),
      m_Circular(false)
{
}


void CPhrap_Contig::Read(CNcbiIstream& in)
{
    CPhrap_Seq::Read(in);
    char flag;
    in >> m_NumReads >> m_NumSegs >> flag;
    CheckStreamState(in, "CO data.");
    SetComplemented(flag == 'C');
}


void CPhrap_Contig::ReadBaseQualities(CNcbiIstream& in)
{
    int bq;
    for (size_t i = 0; i < GetUnpaddedLength(); i++) {
        in >> bq;
        m_BaseQuals.push_back(bq);
    }
    CheckStreamState(in, "BQ data.");
    _ASSERT( isspace(in.peek()) );
}


void CPhrap_Contig::ReadReadLocation(CNcbiIstream& in)
{
    string name;
    char c;
    TSignedSeqPos start;
    in >> name >> c >> start;
    CheckStreamState(in, "AF data.");
    start--;
    CRef<CPhrap_Read>& read = m_Reads[name];
    if ( !read ) {
        read.Reset(new CPhrap_Read(GetFlags()));
    }
    read->AddReadLoc(start, c == 'C'); 
    if (start < 0) {
        // Add second location
        start += GetPaddedLength();
        read->AddReadLoc(start, c == 'C');
        // Mark the contig as circular
        m_Circular = true;
    }
}


void CPhrap_Contig::ReadBaseSegment(CNcbiIstream& in)
{
    SBaseSeg seg;
    string name;
    in >> seg.m_Start >> seg.m_End >> name;
    CheckStreamState(in, "BS data.");
    seg.m_Start--;
    seg.m_End--;
    m_BaseSegMap[name].push_back(seg);
}


void CPhrap_Contig::ReadTag(CNcbiIstream& in, char tag)
{
    _ASSERT(tag == 'C');
    SContigTag ct;
    string data = ReadLine(in);
    list<string> fields;
    NStr::Split(data, " ", fields);
    list<string>::const_iterator f = fields.begin();

    // Need some tricks to get optional NoTrans flag
    if (f == fields.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: incomplete CT tag for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    ct.m_Type = *f;
    f++;
    if (f == fields.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: incomplete CT tag for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    ct.m_Program = *f;
    f++;
    if (f == fields.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: incomplete CT tag for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    ct.m_Start = NStr::StringToInt(*f);
    if (ct.m_Start > 0) {
        ct.m_Start--;
    }
    f++;
    if (f == fields.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: incomplete CT tag for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    ct.m_End = NStr::StringToInt(*f);
    if (ct.m_End > 0) {
        ct.m_End--;
    }
    f++;
    if (f == fields.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: incomplete CT tag for " + GetName() + ".",
                    in.tellg() - CT_POS_TYPE(0));
    }
    ct.m_Date = *f;
    f++;
    ct.m_NoTrans = (f != fields.end()  &&  *f == "NoTrans");
    in >> ws;

    // Read oligo tag: <oligo_name> <(stop-start+1) bases> <melting temp> <U|C>
    if (ct.m_Type == "oligo") {
        char c;
        in  >> ct.m_Oligo.m_Name
            >> ct.m_Oligo.m_Data
            >> ct.m_Oligo.m_MeltTemp
            >> c
            >> ws;
        CheckStreamState(in, "CT{} oligo data.");
        ct.m_Oligo.m_Complemented = (c == 'C');
        if (ct.m_Oligo.m_Data.size() != ct.m_End - ct.m_Start + 1) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: invalid oligo data length.",
                        in.tellg() - CT_POS_TYPE(0));
        }
    }
    // Read all lines untill closing '}'
    for (string c = ReadLine(in); c != "}"; c = ReadLine(in)) {
        ct.m_Comments.push_back(c);
    }
    m_Tags.push_back(ct);
}


CRef<CPhrap_Read>& CPhrap_Contig::SetRead(const CPhrap_Read& read)
{
    CRef<CPhrap_Read>& ret = m_Reads[read.GetName()];
    if ( !ret ) {
        ret.Reset(new CPhrap_Read(GetFlags()));
    }
    ret->CopyFrom(read);
    return ret;
}


void CPhrap_Contig::x_CreateGraph(CBioseq& bioseq) const
{
    if ( m_BaseQuals.empty() ) {
        return;
    }
    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetTitle("Phrap Quality");
    graph->SetLoc().SetWhole().SetLocal().SetStr(GetName());
    graph->SetNumval(GetUnpaddedLength());
    CByte_graph::TValues& values = graph->SetGraph().SetByte().SetValues();
    values.resize(GetUnpaddedLength());
    int max_val = 0;
    for (size_t i = 0; i < GetUnpaddedLength(); i++) {
        values[i] = m_BaseQuals[i];
        if (m_BaseQuals[i] > max_val) {
            max_val = m_BaseQuals[i];
        }
    }
    graph->SetGraph().SetByte().SetMin(0);
    graph->SetGraph().SetByte().SetMax(max_val);
    graph->SetGraph().SetByte().SetAxis(0);

    annot->SetData().SetGraph().push_back(graph);
    bioseq.SetAnnot().push_back(annot);
}


bool CPhrap_Contig::x_AddAlignRanges(TSeqPos           global_start,
                                     TSeqPos           global_stop,
                                     const CPhrap_Seq& seq,
                                     size_t            seq_idx,
                                     TSignedSeqPos     offset,
                                     TAlignMap&        aln_map,
                                     TAlignStarts&     aln_starts) const
{
    if (global_start >= seq.GetUnpaddedLength() + offset) {
        return false;
    }
    bool ret = false;
    TSeqPos pstart = max(offset, TSignedSeqPos(global_start));
    // Make special method to do all the work and return the
    // correct initial pad iterator.
    TSeqPos ustart = seq.GetUnpaddedPos(pstart - offset, &pstart);
    if (ustart == kInvalidSeqPos) {
        return false;
    }
    const TPadMap& pads = seq.GetPadMap();
    SAlignInfo info(seq_idx);
    TAlignMap::range_type rg;
    ITERATE(TPadMap, pad_it, pads) {
        TSeqPos pad = pad_it->first - pad_it->second;
        if (pad <= ustart) {
            if (ret) pstart++;
            continue;
        }
        if (pstart >= GetPaddedLength() || pstart >= global_stop) {
            break;
        }
        TSeqPos len = pad - ustart;
        if (pstart + len > global_stop) {
            len = global_stop - pstart;
        }
        rg.Set(pstart, pstart + len - 1);
        pstart += len + 1; // +1 to skip gap
        info.m_Start = ustart;
        ustart += len;
        aln_starts.insert(rg.GetFrom());
        aln_starts.insert(rg.GetToOpen());
        aln_map.insert(TAlignMap::value_type(rg, info));
        ret = true;
    }
    _ASSERT(seq.GetUnpaddedLength() >= ustart);
    TSeqPos len = seq.GetUnpaddedLength() - ustart;
    if (len > 0  &&  pstart < global_stop) {
        if (pstart + len > global_stop) {
            len = global_stop - pstart;
        }
        rg.Set(pstart, pstart + len - 1);
        if (rg.GetFrom() < GetPaddedLength()) {
            info.m_Start = ustart;
            aln_starts.insert(rg.GetFrom());
            aln_starts.insert(rg.GetToOpen());
            aln_map.insert(TAlignMap::value_type(rg, info));
            ret = true;
        }
    }
    return ret;
}


CRef<CSeq_align> CPhrap_Contig::x_CreateSeq_align(TAlignMap&     aln_map,
                                                  TAlignStarts&  aln_starts,
                                                  TAlignRows&    rows) const
{
    size_t dim = rows.size();
    if ( dim < 2 ) {
        return CRef<CSeq_align>(0);
    }
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial); // ???
    align->SetDim(dim); // contig + one reads
    CDense_seg& dseg = align->SetSegs().SetDenseg();
    dseg.SetDim(dim);
    ITERATE(TAlignRows, row, rows) {
        dseg.SetIds().push_back((*row)->GetId());
    }
    size_t numseg = 0;
    size_t data_size = 0;
    CDense_seg::TStarts& starts = dseg.SetStarts();
    CDense_seg::TStrands& strands = dseg.SetStrands();
    starts.resize(dim*aln_starts.size(), -1);
    strands.resize(starts.size(), eNa_strand_unknown);
    TAlignStarts::const_iterator seg_end = aln_starts.begin();
    ITERATE(TAlignStarts, seg_start, aln_starts) {
        if (*seg_start >= GetPaddedLength()) {
            break;
        }
        ++seg_end;
        TAlignMap::iterator rg_it =
            aln_map.begin(TAlignMap::range_type(*seg_start, *seg_start));
        if ( !rg_it ) {
            // Skip global gap
            continue;
        }
        _ASSERT(seg_end != aln_starts.end());
        size_t row_count = 0;
        for ( ; rg_it; ++rg_it) {
            row_count++;
            const TAlignMap::range_type& aln_rg = rg_it->first;
            const SAlignInfo& info = rg_it->second;
            size_t idx = data_size + info.m_SeqIndex;
            const CPhrap_Seq& seq = *rows[info.m_SeqIndex];
            if (seq.IsComplemented()) {
                starts[idx] =
                    seq.GetUnpaddedLength() -
                    info.m_Start + aln_rg.GetFrom() - *seg_end;
                //strands[idx] = eNa_strand_minus;
            }
            else {
                starts[idx] = info.m_Start + *seg_start - aln_rg.GetFrom();
                //strands[idx] = eNa_strand_plus;
            }
        }
        if (row_count < 2) {
            // Need at least 2 sequences to align
            continue;
        }
        for (size_t row = 0; row < dim; row++) {
            strands[data_size + row] = (rows[row]->IsComplemented()) ?
                eNa_strand_minus : eNa_strand_plus;
        }
        dseg.SetLens().push_back(*seg_end - *seg_start);
        numseg++;
        data_size += dim;
    }
    starts.resize(data_size);
    strands.resize(data_size);
    dseg.SetNumseg(numseg);
    return align;
}


void CPhrap_Contig::x_CreateAlign(CBioseq& bioseq) const
{
    if ( m_Reads.empty() ) {
        return;
    }
    switch ( GetFlags() & fPhrap_Align ) {
    case fPhrap_AlignAll:
        x_CreateAlignAll(bioseq);
        break;
    case fPhrap_AlignPairs:
        x_CreateAlignPairs(bioseq);
        break;
    case fPhrap_AlignOptimized:
        x_CreateAlignOptimized(bioseq);
        break;
    }
}


void CPhrap_Contig::x_CreateAlignAll(CBioseq& bioseq) const
{
    CRef<CSeq_annot> annot(new CSeq_annot);

    // Align unpadded contig and each unpadded read to padded contig coords
    TAlignMap aln_map;
    TAlignStarts aln_starts;
    TAlignRows rows;
    size_t dim = 0;
    TSeqPos global_start = 0;
    TSeqPos global_stop = GetPaddedLength();
    if ( x_AddAlignRanges(global_start, global_stop,
        *this, 0, 0, aln_map, aln_starts) ) {
        rows.push_back(CConstRef<CPhrap_Seq>(this));
        dim = 1;
    }
    ITERATE (TReads, rd, m_Reads) {
        const CPhrap_Read& read = *rd->second;
        for (size_t loc = 0; loc < read.GetStarts().size(); loc++) {
            TSignedSeqPos start = read.GetStarts()[loc];
            if (x_AddAlignRanges(global_start, global_stop,
                read, dim, start, aln_map, aln_starts)) {
                dim++;
                rows.push_back(CConstRef<CPhrap_Seq>(&read));
            }
        }
    }
    CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
    if  ( !align ) {
        return;
    }
    annot->SetData().SetAlign().push_back(align);
    bioseq.SetAnnot().push_back(annot);
}


void CPhrap_Contig::x_CreateAlignPairs(CBioseq& bioseq) const
{
    // One-to one version
    CRef<CSeq_annot> annot(new CSeq_annot);
    ITERATE(TReads, rd, m_Reads) {
        TAlignMap aln_map;
        TAlignStarts aln_starts;
        TAlignRows rows;
        const CPhrap_Read& read = *rd->second;

        size_t dim = 1;
        rows.push_back(CConstRef<CPhrap_Seq>(this));
        // Align unpadded contig and each loc of each read to padded coords
        ITERATE(CPhrap_Read::TStarts, offset, read.GetStarts()) {
            TSignedSeqPos global_start = *offset < 0 ? 0 : *offset;
            TSignedSeqPos global_stop = read.GetPaddedLength() + *offset;
            x_AddAlignRanges(global_start, global_stop,
                *this, 0, 0, aln_map, aln_starts);
            if ( x_AddAlignRanges(global_start, global_stop,
                read, dim, *offset, aln_map, aln_starts) ) {
                rows.push_back(CConstRef<CPhrap_Seq>(&read));
                dim++;
            }
        }
        CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
        if  ( !align ) {
            continue;
        }
        annot->SetData().SetAlign().push_back(align);
    }
    bioseq.SetAnnot().push_back(annot);
}


const TSeqPos kMaxSegLength = 100000;

void CPhrap_Contig::x_CreateAlignOptimized(CBioseq& bioseq) const
{
    // Optimized (diagonal) set of alignments
    CRef<CSeq_annot> annot(new CSeq_annot);

    for (TSeqPos g_start = 0; g_start < GetPaddedLength();
        g_start += kMaxSegLength) {
        TSeqPos g_stop = g_start + kMaxSegLength;
        TAlignMap aln_map;
        TAlignStarts aln_starts;
        TAlignRows rows;
        size_t dim = 0;
        if ( x_AddAlignRanges(g_start, g_stop,
            *this, 0, 0, aln_map, aln_starts) ) {
            rows.push_back(CConstRef<CPhrap_Seq>(this));
            dim = 1;
        }
        ITERATE (TReads, rd, m_Reads) {
            const CPhrap_Read& read = *rd->second;
            for (size_t loc = 0; loc < read.GetStarts().size(); loc++) {
                TSignedSeqPos start = read.GetStarts()[loc];
                if (x_AddAlignRanges(g_start, g_stop,
                    read, dim, start, aln_map, aln_starts)) {
                    dim++;
                    rows.push_back(CConstRef<CPhrap_Seq>(&read));
                }
            }
        }
        CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
        if  ( !align ) {
            continue;
        }
        annot->SetData().SetAlign().push_back(align);
    }
    bioseq.SetAnnot().push_back(annot);
}


void CPhrap_Contig::x_AddBaseSegFeats(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatBaseSegs)  ||  m_BaseSegMap.empty() ) {
        return;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    ITERATE(TBaseSegMap, bs_set, m_BaseSegMap) {
        CRef<CPhrap_Read> read = m_Reads[bs_set->first];
        if ( !read ) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: referenced read " + bs_set->first + " not found.",
                        CT_POS_TYPE(-1));
        }
        ITERATE(TBaseSegs, bs, bs_set->second) {
            TSignedSeqPos rd_start;
            _ASSERT(!read->GetStarts().empty());
            for (size_t i = 0; i < read->GetStarts().size(); i++) {
                TSignedSeqPos start = read->GetStarts()[i];
                if (start >= 0) {
                    if (bs->m_Start >= start &&
                        bs->m_End <= start + read->GetPaddedLength()) {
                        rd_start = start;
                        break;
                    }
                }
                else {
                    if (bs->m_End <= start + read->GetPaddedLength()) {
                        rd_start = start;
                        break;
                    }
                }
            }
            TSeqPos start = bs->m_Start - rd_start;
            TSeqPos stop = bs->m_End - rd_start;
            start = read->GetUnpaddedPos(start);
            stop = read->GetUnpaddedPos(stop);
            _ASSERT(start != kInvalidSeqPos);
            _ASSERT(stop != kInvalidSeqPos);
            CRef<CSeq_feat> bs_feat(new CSeq_feat);
            bs_feat->SetData().SetImp().SetKey("base_segment");                // ???
            CSeq_loc& loc = bs_feat->SetLocation();
            loc.SetInt().SetId(*read->GetId());
            if ( read->IsComplemented() ) {
                loc.SetInt().SetFrom(read->GetUnpaddedLength() - stop - 1);
                loc.SetInt().SetTo(read->GetUnpaddedLength() - start - 1);
                loc.SetInt().SetStrand(eNa_strand_minus);
            }
            else {
                loc.SetInt().SetFrom(start);
                loc.SetInt().SetTo(stop);
            }
            start = GetUnpaddedPos(bs->m_Start);
            stop = GetUnpaddedPos(bs->m_End);
            _ASSERT(start != kInvalidSeqPos);
            _ASSERT(stop != kInvalidSeqPos);
            CSeq_loc& prod = bs_feat->SetProduct();
            prod.SetInt().SetId(*GetId());
            prod.SetInt().SetFrom(start);
            prod.SetInt().SetTo(stop);
            annot->SetData().SetFtable().push_back(bs_feat);
        }
    }
}


void CPhrap_Contig::x_AddReadLocFeats(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatReadLocs)  ||  m_Reads.empty() ) {
        return;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    ITERATE(TReads, read, m_Reads) {
        for (size_t i = 0; i < read->second->GetStarts().size(); i++) {
            TSignedSeqPos rd_start = read->second->GetStarts()[i];
            if (rd_start < 0) {
                // Add only positive starts
                continue;
            }
            CRef<CSeq_feat> loc_feat(new CSeq_feat);
            loc_feat->SetExcept(true);
            loc_feat->SetExcept_text(
                "Coordinates are specified for padded read and contig");
            loc_feat->SetData().SetImp().SetKey("read_start");                           // ???
            CSeq_loc& loc = loc_feat->SetLocation();
            loc.SetPnt().SetId(*read->second->GetId());
            if ( read->second->IsComplemented() ) {
                loc.SetPnt().SetPoint(read->second->GetPaddedLength() - 1);
                loc.SetPnt().SetStrand(eNa_strand_minus);
            }
            else {
                loc.SetPnt().SetPoint(0);
            }
            CSeq_loc& prod = loc_feat->SetProduct();
            prod.SetPnt().SetId(*GetId());
            prod.SetPnt().SetPoint(rd_start);
            annot->SetData().SetFtable().push_back(loc_feat);
        }
    }
}


void CPhrap_Contig::x_AddTagFeats(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatTags)  ||  m_Tags.empty() ) {
        return;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    ITERATE(TContigTags, tag_it, m_Tags) {
        const SContigTag& tag = *tag_it;
        CRef<CSeq_feat> feat(new CSeq_feat);
        string& title = feat->SetTitle();
        title = "created " + tag.m_Date + " by " + tag.m_Program;
        if ( tag.m_NoTrans ) {
            title += " (NoTrans)";
        }
        string comment;
        ITERATE(vector<string>, c, tag.m_Comments) {
            comment += (comment.empty() ? "" : " | ") + *c;
        }
        if ( !comment.empty() ) {
            feat->SetComment(comment);
        }
        feat->SetData().SetImp().SetKey(tag.m_Type);                           // ???
        if ( !tag.m_Oligo.m_Name.empty() ) {
            feat->SetData().SetImp().SetDescr(
                tag.m_Oligo.m_Name + " " +
                tag.m_Oligo.m_Data + " " +
                tag.m_Oligo.m_MeltTemp + " " +
                (tag.m_Oligo.m_Complemented ? "C" : "U"));
        }
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        loc.SetInt().SetFrom(GetUnpaddedPos(tag.m_Start));
        loc.SetInt().SetTo(GetUnpaddedPos(tag.m_End));
        annot->SetData().SetFtable().push_back(feat);
    }
}


void CPhrap_Contig::x_CreateFeat(CBioseq& bioseq) const
{
    CRef<CSeq_annot> annot;
    CreatePadsFeat(annot);
    x_AddReadLocFeats(annot);
    x_AddBaseSegFeats(annot);
    x_AddTagFeats(annot);
    if ( annot ) {
        bioseq.SetAnnot().push_back(annot);
    }
}


void CPhrap_Contig::x_CreateDesc(CBioseq& bioseq) const
{
    if ( !FlagSet(fPhrap_Descr) ) {
        return;
    }
    return;
}


CRef<CSeq_entry> CPhrap_Contig::CreateContig(int level) const
{
    CRef<CSeq_entry> cont_entry(new CSeq_entry);
    CRef<CBioseq> bioseq = CreateBioseq();
    _ASSERT(bioseq);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_consen);                        // ???
    if ( m_Circular ) {
        bioseq->SetInst().SetTopology(CSeq_inst::eTopology_circular);
    }
    cont_entry->SetSeq(*bioseq);

    x_CreateDesc(*bioseq);
    x_CreateGraph(*bioseq);
    x_CreateAlign(*bioseq);
    x_CreateFeat(*bioseq);

    CRef<CSeq_entry> set_entry(new CSeq_entry);
    CBioseq_set& bioseq_set = set_entry->SetSet();
    bioseq_set.SetLevel(level);
    bioseq_set.SetClass(CBioseq_set::eClass_conset);                           // ???
    bioseq_set.SetSeq_set().push_back(cont_entry);
    ITERATE(TReads, it, m_Reads) {
        CRef<CSeq_entry> rd_entry = it->second->CreateRead();
        bioseq_set.SetSeq_set().push_back(rd_entry);
    }
    return set_entry;
}


class CPhrapReader
{
public:
    CPhrapReader(CNcbiIstream& in, TPhrapReaderFlags flags);
    CRef<CSeq_entry> Read(void);

private:
    enum EPhrapTag {
        ePhrap_not_set, // empty value for m_LastTag
        ePhrap_unknown, // unknown tag (error)
        ePhrap_eof,     // end of file
        ePhrap_AS, // Header: <contigs in file> <reads in file>
        ePhrap_CO, // Contig: <name> <# bases> <# reads> <# base segments> <U or C>
        ePhrap_BQ, // Base qualities for the unpadded consensus bases
        ePhrap_AF, // Location of the read in the contig:
                   // <read> <C or U> <padded start consensus position>
        ePhrap_BS, // Base segment:
                   // <padded start position> <padded end position> <read name>
        ePhrap_RD, // Read:
                   // <name> <# padded bases> <# whole read info items> <# read tags>
        ePhrap_QA, // Quality alignment:
                   // <qual start> <qual end> <align start> <align end>
        ePhrap_DS, // Original data
        ePhrap_RT, // {...}
        ePhrap_CT, // {...}
        ePhrap_WA  // {...}
    };

    struct SAssmTag
    {
        string          m_Type;
        string          m_Program;
        string          m_Date;
        vector<string>  m_Comments;
    };
    typedef vector<SAssmTag> TAssmTags;

    void x_ReadContig();
    void x_ReadRead(CPhrap_Contig& contig);
    void x_ReadTag(string tag); // CT{} and RT{}
    void x_ReadWA(void);        // WA{}

    EPhrapTag x_GetTag(void);
    void x_UngetTag(EPhrapTag tag);

    CPhrap_Seq& x_FindSeq(const string& name);

    void x_CreateDesc(CBioseq_set& bioseq) const;

    typedef vector< CRef<CPhrap_Contig> >   TContigs;
    typedef map<string, CRef<CPhrap_Seq> >  TSeqs;

    CNcbiIstream&     m_Stream;
    TPhrapReaderFlags m_Flags;
    EPhrapTag         m_LastTag;
    CRef<CSeq_entry>  m_Entry;
    size_t            m_NumContigs;
    size_t            m_NumReads;
    TContigs          m_Contigs;
    TSeqs             m_Seqs;
    TAssmTags         m_AssmTags;
};


CPhrapReader::CPhrapReader(CNcbiIstream& in, TPhrapReaderFlags flags)
    : m_Stream(in),
      m_Flags(flags),
      m_LastTag(ePhrap_not_set),
      m_NumContigs(0),
      m_NumReads(0)
{
    return;
}


CRef<CSeq_entry> CPhrapReader::Read(void)
{
    if ( !m_Stream ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: input stream no longer valid",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    EPhrapTag tag = x_GetTag();
    if (tag != ePhrap_AS) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: invalid data, AS tag expected.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }

    for (size_t i = 0; i < m_NumContigs; i++) {
        x_ReadContig();
    }
    _ASSERT(m_Contigs.size() == m_NumContigs);

    if (x_GetTag() != ePhrap_eof) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: unrecognized extra-data, EOF expected.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }

    if (m_Contigs.size() == 1) {
        m_Entry = m_Contigs[0]->CreateContig(1);
        x_CreateDesc(m_Entry->SetSet());
    }
    else {
        m_Entry.Reset(new CSeq_entry);
        CBioseq_set& bset = m_Entry->SetSet();
        bset.SetLevel(1);
        //bset.SetClass(...);
        //bset.SetDescr(...);
        for (size_t i = 0; i < m_Contigs.size(); i++) {
            bset.SetSeq_set().push_back(m_Contigs[i]->CreateContig(2));
        }
        x_CreateDesc(m_Entry->SetSet());
    }

    return m_Entry;
}


void CPhrapReader::x_UngetTag(EPhrapTag tag)
{
    _ASSERT(m_LastTag == ePhrap_not_set);
    m_LastTag = tag;
}


CPhrapReader::EPhrapTag CPhrapReader::x_GetTag(void)
{
    if (m_LastTag != ePhrap_not_set) {
        EPhrapTag ret = m_LastTag;
        m_LastTag = ePhrap_not_set;
        return ret;
    }
    m_Stream >> ws;
    if ( m_Stream.eof() ) {
        return ePhrap_eof;
    }
    switch (m_Stream.get()) {
    case 'A': // AS, AF
        switch (m_Stream.get()) {
        case 'F':
            return ePhrap_AF;
        case 'S':
            // No duplicate 'AS' tags
            if (m_NumContigs != 0) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            "ReadPhrap: duplicate AS tag.",
                            m_Stream.tellg() - CT_POS_TYPE(0));
            }
            m_Stream >> m_NumContigs >> m_NumReads;
            CheckStreamState(m_Stream, "invalid data in AS tag.");
            return ePhrap_AS;
        }
        break;
    case 'B': // BQ, BS
        switch (m_Stream.get()) {
        case 'S':
            return ePhrap_BS;
        case 'Q':
            return ePhrap_BQ;
        }
        break;
    case 'C': // CO, CT
        switch (m_Stream.get()) {
        case 'O':
            return ePhrap_CO;
        case 'T':
            return ePhrap_CT;
        }
        break;
    case 'D': // DS
        if (m_Stream.get() == 'S') {
            return ePhrap_DS;
        }
        break;
    case 'Q': // QA
        if (m_Stream.get() == 'A') {
            return ePhrap_QA;
        }
        break;
    case 'R': // RD, RT
        switch (m_Stream.get()) {
        case 'D':
            return ePhrap_RD;
        case 'T':
            return ePhrap_RT;
        }
        break;
    case 'W': // WA
        if (m_Stream.get() == 'A') {
            return ePhrap_WA;
        }
        break;
    }
    CheckStreamState(m_Stream, "tag.");
    NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: unknown tag.",
                m_Stream.tellg() - CT_POS_TYPE(0));
    return ePhrap_unknown;
}


inline
CPhrap_Seq& CPhrapReader::x_FindSeq(const string& name)
{
    TSeqs::iterator seq = m_Seqs.find(name);
    if (seq == m_Seqs.end()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: referenced contig or read not found: " + name + ".",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    return *seq->second;
}


void CPhrapReader::x_ReadTag(string tag)
{
    m_Stream >> ws;
    if (m_Stream.get() != '{') {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: '{' expected after " + tag + " tag.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    string name;
    m_Stream >> name;
    CheckStreamState(m_Stream, tag + "{} data.");
    CPhrap_Seq& seq = x_FindSeq(name);
    seq.ReadTag(m_Stream, tag[0]);
}


void CPhrapReader::x_ReadWA(void)
{
    m_Stream >> ws;
    if (m_Stream.get() != '{') {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: '{' expected after WA tag.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    SAssmTag wt;
    m_Stream
        >> wt.m_Type
        >> wt.m_Program
        >> wt.m_Date
        >> ws;
    CheckStreamState(m_Stream, "WA{} data.");
    // Read all lines untill closing '}'
    for (string c = NStr::TruncateSpaces(ReadLine(m_Stream));
        c != "}"; c = NStr::TruncateSpaces(ReadLine(m_Stream))) {
        wt.m_Comments.push_back(c);
    }
    m_AssmTags.push_back(wt);
}


void CPhrapReader::x_ReadContig(void)
{
    EPhrapTag tag = x_GetTag();
    if (tag != ePhrap_CO) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: invalid data, contig tag expected.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    CRef<CPhrap_Contig> contig(new CPhrap_Contig(m_Flags));
    contig->Read(m_Stream);
    contig->ReadData(m_Stream);
    m_Contigs.push_back(contig);
    m_Seqs[contig->GetName()] = contig;
    for (EPhrapTag tag = x_GetTag(); tag != ePhrap_eof; tag = x_GetTag()) {
        switch ( tag ) {
        case ePhrap_BQ:
            contig->ReadBaseQualities(m_Stream);
            continue;
        case ePhrap_AF:
            contig->ReadReadLocation(m_Stream);
            continue;
        case ePhrap_BS:
            contig->ReadBaseSegment(m_Stream);
            continue;
        case ePhrap_eof:
            return;
        default:
            x_UngetTag(tag);
        }
        break;
    }
    // Read to the next contig or eof:
    while ((tag = x_GetTag()) != ePhrap_eof) {
        switch ( tag ) {
        case ePhrap_RD:
            x_ReadRead(*contig);
            continue;
        case ePhrap_RT:
            x_ReadTag("RT");
            continue;
        case ePhrap_CT:
            x_ReadTag("CT");
            continue;
        case ePhrap_WA:
            x_ReadWA();
            continue;
        case ePhrap_eof:
            return;
        default:
            x_UngetTag(tag);
        }
        break;
    }
}


void CPhrapReader::x_ReadRead(CPhrap_Contig& contig)
{
    CPhrap_Read tmp_read(m_Flags);
    tmp_read.Read(m_Stream);
    CRef<CPhrap_Read> read = contig.SetRead(tmp_read);
    read->ReadData(m_Stream);
    m_Seqs[read->GetName()] = read;
    for (EPhrapTag tag = x_GetTag(); tag != ePhrap_eof; tag = x_GetTag()) {
        switch ( tag ) {
        case ePhrap_QA:
            read->ReadQuality(m_Stream);
            break;
        case ePhrap_DS:
            read->ReadDS(m_Stream);
            break;
        case ePhrap_eof:
            return;
        default:
            x_UngetTag(tag);
            return;
        }
    }
}


void CPhrapReader::x_CreateDesc(CBioseq_set& bioseq_set) const
{
    if ( (m_Flags & fPhrap_Descr == 0)  ||  m_AssmTags.empty() ) {
        return;
    }
    CRef<CSeq_descr> descr(new CSeq_descr);
    CRef<CSeqdesc> desc;

    ITERATE(TAssmTags, tag, m_AssmTags) {
        desc.Reset(new CSeqdesc);
        string comment;
        ITERATE(vector<string>, c, tag->m_Comments) {
            comment += " | " + *c;
        }
        desc->SetComment(
            tag->m_Type + " " +
            tag->m_Program + " " +
            tag->m_Date +
            comment);
        descr->Set().push_back(desc);
    }
    bioseq_set.SetDescr(*descr);
}


CRef<CSeq_entry> ReadPhrap(CNcbiIstream& in, TPhrapReaderFlags flags)
{
    CPhrapReader reader(in, flags);
    return reader.Read();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/05/02 13:10:18  grichenk
* Initial revision
*
*
* ===========================================================================
*/
