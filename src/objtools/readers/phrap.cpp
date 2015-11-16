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
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Phrap

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Read whole line from a stream
inline
string ReadLine(CNcbiIstream& in)
{
    in >> ws;
    string ret;
    getline(in, ret);
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


inline bool IsOldComplementedName(const string& name)
{
    // In old ACE complemented reads have names ending with  '.comp'
    const string kOldNameCompFlag = ".comp";
    return NStr::Find(name, kOldNameCompFlag, NStr::eLast) ==
        name.size() - kOldNameCompFlag.size();
}


class CPhrap_Seq : public CObject
{
public:
    CPhrap_Seq(TPhrapReaderFlags flags);
    CPhrap_Seq(const string& name, TPhrapReaderFlags flags);
    virtual ~CPhrap_Seq(void) {}

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

    TSeqPos GetAlignedFrom(void) const { return m_AlignedFrom; }
    TSeqPos GetAlignedTo(void) const { return m_AlignedTo; }

protected:
    void CreateComplementedDescr(CRef<CSeq_descr>& descr) const;
    void CreatePadsFeat(CRef<CSeq_annot>& annot) const;
    void SetAligned(TSeqPos from, TSeqPos to)
        {
            m_AlignedFrom = from;
            m_AlignedTo = to;
        }

private:
    void x_FillSeqData(CSeq_data& data) const;

    friend class CPhrap_Sequence;
    void CopyFrom(CPhrap_Seq& seq);

    TPhrapReaderFlags m_Flags;

    string          m_Name;
    TSeqPos         m_PaddedLength;
    TSeqPos         m_UnpaddedLength;
    string          m_Data;   // pads are already removed
    TPadMap         m_PadMap; // shifts for unpadded positions
    bool            m_Complemented;
    TSeqPos         m_AlignedFrom;
    TSeqPos         m_AlignedTo;
    mutable CRef<CSeq_id> m_Id;
};


const char kPadChar = '*';

CPhrap_Seq::CPhrap_Seq(TPhrapReaderFlags flags)
    : m_Flags(flags),
      m_PaddedLength(0),
      m_UnpaddedLength(0),
      m_Complemented(false),
      m_AlignedFrom(0),
      m_AlignedTo(kInvalidSeqPos)
{
}


CPhrap_Seq::CPhrap_Seq(const string& name, TPhrapReaderFlags flags)
    : m_Flags(flags),
      m_Name(name),
      m_PaddedLength(0),
      m_UnpaddedLength(0),
      m_Complemented(false),
      m_AlignedFrom(0),
      m_AlignedTo(kInvalidSeqPos)
{
}


void CPhrap_Seq::CopyFrom(CPhrap_Seq& seq)
{
    m_Flags = seq.m_Flags;
    m_Name = seq.m_Name;
    m_PaddedLength = seq.m_PaddedLength;
    m_UnpaddedLength = seq.m_UnpaddedLength;
    _ASSERT(m_Data.empty());
    m_Data.swap(seq.m_Data);
    _ASSERT(m_PadMap.empty());
    m_PadMap.swap(seq.m_PadMap);
    m_Complemented = seq.m_Complemented;
    m_AlignedFrom = seq.m_AlignedFrom;
    m_AlignedTo = seq.m_AlignedTo;
    m_Id = seq.m_Id;
}


void CPhrap_Seq::Read(CNcbiIstream& in)
{
    if ( m_Name.empty() ) {
        in >> m_Name;
        CheckStreamState(in, "sequence header.");
    }
    in >> m_PaddedLength;
    CheckStreamState(in, "sequence header.");
}


void CPhrap_Seq::ReadData(CNcbiIstream& in)
{
    _ASSERT(m_Data.empty());
    string line;
    TSeqPos cnt = 0;
    if ((m_Flags & fPhrap_OldVersion) != 0) {
        // Prepare to read as many bases as possible
        m_PaddedLength = kInvalidSeqPos;
    }
    while (!in.eof()  &&  cnt < m_PaddedLength) {
        // in >> line;
        line = ReadLine(in);
        char c = in.peek();
        m_Data += NStr::ToUpper(line);
        cnt += line.size();
        if ((m_Flags & fPhrap_OldVersion) != 0  &&  isspace(c)) {
            break;
        }
    }
    if ((m_Flags & fPhrap_OldVersion) != 0) {
        m_PaddedLength = cnt;
    }
    char next = in.eof() ? ' ' : in.peek();
    if ( m_Data.size() != m_PaddedLength  || !isspace((unsigned char) next) ) {
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
    m_AlignedTo = m_PaddedLength - 1;
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
    inst.SetMol(CSeq_inst::eMol_dna);
    inst.SetLength(m_UnpaddedLength);

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


void CPhrap_Seq::CreateComplementedDescr(CRef<CSeq_descr>& descr) const
{
    if ( m_Complemented ) {
        if ( !descr ) {
            descr.Reset(new CSeq_descr);
        }
        CRef<CSeqdesc> desc(new CSeqdesc);
        if ( FlagSet(fPhrap_NoComplement) ) {
            // Should be complemented, ignored due to options selected
            desc->SetComment("Complemented flag ignored");
        }
        else {
            // The sequence is complemented
            desc->SetComment("Complemented");
        }
        descr->Set().push_back(desc);
    }
}


void CPhrap_Seq::CreatePadsFeat(CRef<CSeq_annot>& annot) const
{
    // One pad is artificial and indicates end of sequence
    if ( !FlagSet(fPhrap_FeatGaps)  ||  m_PadMap.size() <= 1 ) {
        return;
    }
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetData().SetImp().SetKey("gap_set");
    feat->SetComment("Gap set for " + m_Name);
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

    CPhrap_Read(const string& name, TPhrapReaderFlags flags);
    virtual ~CPhrap_Read(void);

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
    typedef vector<SReadTag>      TReadTags;
    typedef TSignedSeqPos         TStart;
    typedef CRange<TSignedSeqPos> TRange;

    void AddReadLoc(TSignedSeqPos start, bool complemented);

    TStart GetStart(void) const { return m_Start; }

    void ReadQuality(CNcbiIstream& in);  // QA
    void ReadDS(CNcbiIstream& in);       // DS
    virtual void ReadTag(CNcbiIstream& in, char tag); // RT{}

    CRef<CSeq_entry> CreateRead(void) const;

    bool IsCircular(void) const;

private:
    void x_CreateFeat(CBioseq& bioseq) const;
    void x_CreateDesc(CBioseq& bioseq) const;
    void x_AddTagFeats(CRef<CSeq_annot>& annot) const;
    void x_AddQualityFeat(CRef<CSeq_annot>& annot) const;

    size_t    m_NumInfoItems;
    size_t    m_NumReadTags;
    TRange    m_HiQualRange;
    TStart    m_Start;
    SReadDS*  m_DS;
    TReadTags m_Tags;
};


CPhrap_Read::CPhrap_Read(const string& name, TPhrapReaderFlags flags)
    : CPhrap_Seq(name, flags),
      m_NumInfoItems(0),
      m_NumReadTags(0),
      m_HiQualRange(TRange::GetEmpty()),
      m_Start(0),
      m_DS(0)
{
}


CPhrap_Read::~CPhrap_Read(void)
{
    if ( m_DS ) {
        delete m_DS;
    }
}


void CPhrap_Read::Read(CNcbiIstream& in)
{
    CPhrap_Seq::Read(in);
    in >> m_NumInfoItems >> m_NumReadTags;
    CheckStreamState(in, "RD data.");
}


bool CPhrap_Read::IsCircular(void) const
{
    return m_Start + (TStart)GetAlignedFrom() < 0;
}


void CPhrap_Read::ReadQuality(CNcbiIstream& in)
{
    TSignedSeqPos start, stop;
    in >> start >> stop;
    CheckStreamState(in, "QA data.");
    if (start > 0  &&  stop > 0) {
        m_HiQualRange.Set(start - 1, stop - 1);
    }
    if ((GetFlags() & fPhrap_OldVersion) != 0) {
        return;
    }
    in >> start >> stop;
    CheckStreamState(in, "QA data.");
    if (start > 0  &&  stop > 0) {
        SetAligned(start - 1, stop - 1);
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
    NStr::Split(tag, " ", values, 0);
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
    _ASSERT(m_Start == 0);
    SetComplemented(complemented);
    m_Start = start;
}


void CPhrap_Read::x_AddTagFeats(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatTags)  ||  m_Tags.empty() ) {
        return;
    }
    if (m_Tags.size() != m_NumReadTags) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "ReadPhrap: invalid number of RT tags for " + GetName() + ".",
                    CT_POS_TYPE(0));
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    ITERATE(TReadTags, tag_it, m_Tags) {
        const SReadTag& tag = *tag_it;
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetTitle("created " + tag.m_Date + " by " + tag.m_Program);
        feat->SetData().SetImp().SetKey(tag.m_Type);
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        TSeqPos unpadded_start = GetUnpaddedPos(tag.m_Start);
        TSeqPos unpadded_end = GetUnpaddedPos(tag.m_End);
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() -
                unpadded_end - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() -
                unpadded_start - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().
                    SetP_m(tag.m_End - unpadded_end);
                loc.SetInt().SetFuzz_to().
                    SetP_m(tag.m_Start - unpadded_start);
            }
        }
        else {
            loc.SetInt().SetFrom(unpadded_start);
            loc.SetInt().SetTo(GetUnpaddedPos(tag.m_End));
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().
                    SetP_m(tag.m_Start - unpadded_start);
                loc.SetInt().SetFuzz_to().
                    SetP_m(tag.m_End - unpadded_end);
            }
        }
        annot->SetData().SetFtable().push_back(feat);
    }
}


void CPhrap_Read::x_AddQualityFeat(CRef<CSeq_annot>& annot) const
{
    if ( !FlagSet(fPhrap_FeatQuality) ) {
        return;
    }
    if ( m_HiQualRange.Empty()  &&  GetAlignedTo() == kInvalidSeqPos ) {
        return;
    }
    if ( !annot ) {
        annot.Reset(new CSeq_annot);
    }
    if ( !m_HiQualRange.Empty() ) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetImp().SetKey("high_quality_segment");
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        TSeqPos start = GetUnpaddedPos(m_HiQualRange.GetFrom());
        TSeqPos stop = GetUnpaddedPos(m_HiQualRange.GetTo());
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() - stop - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() - start - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().
                    SetP_m(m_HiQualRange.GetTo() - stop);
                loc.SetInt().SetFuzz_to().
                    SetP_m(m_HiQualRange.GetFrom() - start);
            }
        }
        else {
            loc.SetInt().SetFrom(start);
            loc.SetInt().SetTo(stop);
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().
                    SetP_m(m_HiQualRange.GetFrom() - start);
                loc.SetInt().SetFuzz_to().
                    SetP_m(m_HiQualRange.GetTo() - stop);
            }
        }
        annot->SetData().SetFtable().push_back(feat);
    }
    if (GetAlignedTo() != kInvalidSeqPos) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetImp().SetKey("aligned_segment");
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId(*GetId());
        TSeqPos start = GetUnpaddedPos(GetAlignedFrom());
        TSeqPos stop = GetUnpaddedPos(GetAlignedTo());
        if ( IsComplemented() ) {
            loc.SetInt().SetFrom(GetUnpaddedLength() - stop - 1);
            loc.SetInt().SetTo(GetUnpaddedLength() - start - 1);
            loc.SetInt().SetStrand(eNa_strand_minus);
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().SetP_m(GetAlignedTo() - stop);
                loc.SetInt().SetFuzz_to().SetP_m(GetAlignedFrom() - start);
            }
        }
        else {
            loc.SetInt().SetFrom(start);
            loc.SetInt().SetTo(stop);
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                loc.SetInt().SetFuzz_from().SetP_m(GetAlignedFrom() - start);
                loc.SetInt().SetFuzz_to().SetP_m(GetAlignedTo() - stop);
            }
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
    CRef<CSeq_descr> descr;

    // Always add desc.comment = "Complemented" to indicate reversed read
    CreateComplementedDescr(descr);

    if ( FlagSet(fPhrap_Descr)  &&  m_DS ) {
        if ( !descr ) {
            descr.Reset(new CSeq_descr);
        }
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
    }
    if ( descr  &&  !descr->Get().empty() ) {
        bioseq.SetDescr(*descr);
    }
}


CRef<CSeq_entry> CPhrap_Read::CreateRead(void) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CRef<CBioseq> bioseq = CreateBioseq();
    _ASSERT(bioseq);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);

    x_CreateDesc(*bioseq);
    x_CreateFeat(*bioseq);

    entry->SetSeq(*bioseq);

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
    typedef map<string, CRef<CPhrap_Seq> >  TSeqs;
    void ReadReadLocation(CNcbiIstream& in, TSeqs& seqs);  // AF
    void ReadBaseSegment(CNcbiIstream& in);   // BS
    virtual void ReadTag(CNcbiIstream& in, char tag); // CT{}

    CRef<CSeq_entry> CreateContig(int level) const;

    bool IsCircular(void) const;

private:
    void x_CreateAlign(CBioseq_set& bioseq_set) const;
    void x_CreateGraph(CBioseq& bioseq) const;
    void x_CreateFeat(CBioseq& bioseq) const;
    void x_CreateDesc(CBioseq& bioseq) const;

    void x_AddBaseSegFeats(CRef<CSeq_annot>& annot) const;
    void x_AddReadLocFeats(CRef<CSeq_annot>& annot) const;
    void x_AddTagFeats(CRef<CSeq_annot>& annot) const;

    void x_CreateAlignPairs(CBioseq_set& bioseq_set) const;
    void x_CreateAlignAll(CBioseq_set& bioseq_set) const;
    void x_CreateAlignOptimized(CBioseq_set& bioseq_set) const;

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
};


CPhrap_Contig::CPhrap_Contig(TPhrapReaderFlags flags)
    : CPhrap_Seq(flags),
      m_NumReads(0),
      m_NumSegs(0)
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
        bq = i;
    }
    CheckStreamState(in, "BQ data.");
    _ASSERT( isspace((unsigned char) in.peek()) );
}


void CPhrap_Contig::ReadReadLocation(CNcbiIstream& in, TSeqs& seqs)
{
    string name;
    bool complemented = false;
    TSignedSeqPos start;
    if ((GetFlags() & fPhrap_OldVersion) == 0) {
        char c;
        in >> name >> c >> start;
        CheckStreamState(in, "AF data.");
        complemented = (c == 'C');
    }
    else {
        TSignedSeqPos stop;
        in >> name >> start >> stop;
        CheckStreamState(in, "Assembled_from data.");
    }
    start--;
    CRef<CPhrap_Read>& read = m_Reads[name];
    if ( !read ) {
        CRef<CPhrap_Seq>& seq = seqs[name];
        if ( seq ) {
            read.Reset(dynamic_cast<CPhrap_Read*>(seq.GetPointer()));
            if ( !read ) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: invalid sequence type (" + GetName() + ").",
                            in.tellg() - CT_POS_TYPE(0));
            }
        }
        else {
            read.Reset(new CPhrap_Read(name, GetFlags()));
            seq = CRef<CPhrap_Seq>(read.GetPointer());
        }
    }
    read->AddReadLoc(start, complemented); 
}


bool CPhrap_Contig::IsCircular(void) const
{
    ITERATE(TReads, read, m_Reads) {
        if ( read->second->IsCircular() ) {
            return true;
        }
    }
    return false;
}


void CPhrap_Contig::ReadBaseSegment(CNcbiIstream& in)
{
    SBaseSeg seg;
    string name;
    in >> seg.m_Start >> seg.m_End >> name;
    if ((GetFlags() & fPhrap_OldVersion) != 0) {
        ReadLine(in);
    }
    CheckStreamState(in, "Base segment data.");
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
    TSeqPos aln_from = seq.GetAlignedFrom();
    TSeqPos aln_len = seq.GetAlignedTo() - aln_from;
    if (global_start >= seq.GetPaddedLength() + offset + aln_from) {
        return false;
    }
    bool ret = false;
    TSeqPos pstart = max(offset + TSignedSeqPos(aln_from),
                         TSignedSeqPos(global_start));
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
        if (len > aln_len) {
            len = aln_len;
        }
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
        if ( (aln_len -= len) == 0) {
            break;
        }
    }
    _ASSERT(seq.GetUnpaddedLength() >= ustart);
    TSeqPos len = min(aln_len, seq.GetUnpaddedLength() - ustart);
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
    align->SetType(CSeq_align::eType_partial);
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


void CPhrap_Contig::x_CreateAlign(CBioseq_set& bioseq_set) const
{
    if ( m_Reads.empty() ) {
        return;
    }
    switch ( GetFlags() & fPhrap_Align ) {
    case fPhrap_AlignAll:
        x_CreateAlignAll(bioseq_set);
        break;
    case fPhrap_AlignPairs:
        x_CreateAlignPairs(bioseq_set);
        break;
    case fPhrap_AlignOptimized:
        x_CreateAlignOptimized(bioseq_set);
        break;
    }
}


void CPhrap_Contig::x_CreateAlignAll(CBioseq_set& bioseq_set) const
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
        TSignedSeqPos start = read.GetStart();
        while ( start < TSignedSeqPos(GetPaddedLength()) ) {
            if (x_AddAlignRanges(global_start, global_stop,
                read, dim, start, aln_map, aln_starts)) {
                dim++;
                rows.push_back(CConstRef<CPhrap_Seq>(&read));
            }
            start += GetPaddedLength();
        }
    }
    CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
    if  ( !align ) {
        return;
    }
    annot->SetData().SetAlign().push_back(align);
    bioseq_set.SetAnnot().push_back(annot);
}


void CPhrap_Contig::x_CreateAlignPairs(CBioseq_set& bioseq_set) const
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
//        ITERATE(CPhrap_Read::TStarts, offset, read.GetStarts()) {
        TSignedSeqPos start = read.GetStart();
        while ( start < TSignedSeqPos(GetPaddedLength()) ) {
            TSignedSeqPos global_start = read.GetStart() < 0 ? 0 : start;
            TSignedSeqPos global_stop = read.GetPaddedLength() + start;
            x_AddAlignRanges(global_start, global_stop,
                *this, 0, 0, aln_map, aln_starts);
            if ( x_AddAlignRanges(global_start, global_stop,
                read, dim, start, aln_map, aln_starts) ) {
                rows.push_back(CConstRef<CPhrap_Seq>(&read));
                dim++;
            }
            start += GetPaddedLength();
        }
        CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
        if  ( !align ) {
            continue;
        }
        annot->SetData().SetAlign().push_back(align);
    }
    bioseq_set.SetAnnot().push_back(annot);
}


const TSeqPos kMaxSegLength = 100000;

void CPhrap_Contig::x_CreateAlignOptimized(CBioseq_set& bioseq_set) const
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
            TSignedSeqPos start = read.GetStart();
            while (start < TSignedSeqPos(GetPaddedLength())) {
                if (x_AddAlignRanges(g_start, g_stop,
                    read, dim, start, aln_map, aln_starts)) {
                    dim++;
                    rows.push_back(CConstRef<CPhrap_Seq>(&read));
                }
                start += GetPaddedLength();
            }
        }
        CRef<CSeq_align> align = x_CreateSeq_align(aln_map, aln_starts, rows);
        if  ( !align ) {
            continue;
        }
        annot->SetData().SetAlign().push_back(align);
    }
    bioseq_set.SetAnnot().push_back(annot);
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
                        CT_POS_TYPE(0));
        }
        ITERATE(TBaseSegs, bs, bs_set->second) {
            TSignedSeqPos rd_start = read->GetStart();
            while (rd_start < TSignedSeqPos(GetPaddedLength())) {
                //TSignedSeqPos aln_start = rd_start + read->GetAlignedFrom();
                TSignedSeqPos aln_stop = rd_start + read->GetAlignedTo();
                if (/*TSignedSeqPos(bs->m_Start) >= aln_start  &&*/
                    TSignedSeqPos(bs->m_End) <= aln_stop) {
                    break;
                }
                rd_start += GetPaddedLength();
            }
            _ASSERT(rd_start < TSignedSeqPos(GetPaddedLength()));
            TSeqPos start = bs->m_Start - rd_start;
            TSeqPos stop = bs->m_End - rd_start;
            start = read->GetUnpaddedPos(start);
            stop = read->GetUnpaddedPos(stop);
            _ASSERT(start != kInvalidSeqPos);
            _ASSERT(stop != kInvalidSeqPos);
            CRef<CSeq_feat> bs_feat(new CSeq_feat);
            bs_feat->SetData().SetImp().SetKey("base_segment");
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
        TSignedSeqPos rd_start = read->second->GetStart() +
            read->second->GetAlignedFrom();
        while (rd_start < 0) {
            rd_start += GetPaddedLength();
        }
        CRef<CSeq_feat> loc_feat(new CSeq_feat);
        loc_feat->SetData().SetImp().SetKey("read_start");
        CSeq_loc& loc = loc_feat->SetLocation();
        TSeqPos aln_rd_start = read->second->GetUnpaddedPos(
            read->second->GetAlignedFrom());
        TSeqPos aln_rd_stop = read->second->GetUnpaddedPos(
            read->second->GetAlignedTo());
        loc.SetInt().SetId(*read->second->GetId());
        loc.SetInt().SetFrom(aln_rd_start);
        loc.SetInt().SetTo(aln_rd_stop - 1);
        if ( read->second->IsComplemented() ) {
            loc.SetInt().SetStrand(eNa_strand_minus);
        }
        if ( FlagSet(fPhrap_PadsToFuzz) ) {
            loc.SetInt().SetFuzz_from().
                SetP_m(read->second->GetAlignedFrom() - aln_rd_start);
            loc.SetInt().SetFuzz_to().
                SetP_m(read->second->GetAlignedTo() - aln_rd_stop);
        }
        CSeq_loc& prod = loc_feat->SetProduct();
        TSignedSeqPos rd_stop = rd_start +
            read->second->GetAlignedTo() - read->second->GetAlignedFrom();
        if (rd_stop >= TSignedSeqPos(GetPaddedLength())) {
            // Circular contig, split ranges
            CRef<CSeq_interval> rg1(new CSeq_interval(*GetId(),
                GetUnpaddedPos(rd_start), GetUnpaddedLength() - 1));
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                rg1->SetFuzz_from().SetP_m(rd_start - rg1->GetFrom());
                rg1->SetFuzz_to().SetP_m(GetPaddedLength() - GetUnpaddedLength());
            }
            prod.SetPacked_int().Set().push_back(rg1);

            CRef<CSeq_interval> rg2(new CSeq_interval(*GetId(),
                0, GetUnpaddedPos(rd_stop - GetPaddedLength())));
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                rg2->SetFuzz_from().SetP_m(0);
                rg2->SetFuzz_to().
                    SetP_m(rd_stop - GetPaddedLength() - rg2->GetTo());
            }
            prod.SetPacked_int().Set().push_back(rg2);
        }
        else {
            prod.SetInt().SetId(*GetId());
            prod.SetInt().SetFrom(GetUnpaddedPos(rd_start));
            prod.SetInt().SetTo(GetUnpaddedPos(rd_stop));
            if ( FlagSet(fPhrap_PadsToFuzz) ) {
                prod.SetInt().SetFuzz_from().
                    SetP_m(rd_start - prod.SetInt().GetFrom());
                prod.SetInt().SetFuzz_to().
                    SetP_m(rd_stop - prod.SetInt().GetTo());
            }
        }
        annot->SetData().SetFtable().push_back(loc_feat);
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
        feat->SetData().SetImp().SetKey(tag.m_Type);
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
        if ( FlagSet(fPhrap_PadsToFuzz) ) {
            loc.SetInt().SetFuzz_from().
                SetP_m(tag.m_Start - loc.SetInt().GetFrom());
            loc.SetInt().SetFuzz_to().
                SetP_m(tag.m_End - loc.SetInt().GetTo());
        }
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
    CRef<CSeq_descr> descr;
    CreateComplementedDescr(descr);

    if ( FlagSet(fPhrap_Descr) ) {
        // Reserved for possible descriptors
    }

    if ( descr  &&  !descr->Get().empty() ) {
        bioseq.SetDescr(*descr);
    }
}


CRef<CSeq_entry> CPhrap_Contig::CreateContig(int level) const
{
    CRef<CSeq_entry> cont_entry(new CSeq_entry);
    CRef<CBioseq> bioseq = CreateBioseq();
    _ASSERT(bioseq);
    bioseq->SetInst().SetRepr(CSeq_inst::eRepr_consen);
    if ( IsCircular() ) {
        bioseq->SetInst().SetTopology(CSeq_inst::eTopology_circular);
    }
    cont_entry->SetSeq(*bioseq);

    x_CreateDesc(*bioseq);
    x_CreateGraph(*bioseq);
    x_CreateFeat(*bioseq);

    CRef<CSeq_entry> set_entry(new CSeq_entry);
    CBioseq_set& bioseq_set = set_entry->SetSet();
    bioseq_set.SetLevel(level);
    bioseq_set.SetClass(CBioseq_set::eClass_conset);
    bioseq_set.SetSeq_set().push_back(cont_entry);
    x_CreateAlign(bioseq_set);
    ITERATE(TReads, it, m_Reads) {
        CRef<CSeq_entry> rd_entry = it->second->CreateRead();
        bioseq_set.SetSeq_set().push_back(rd_entry);
    }
    return set_entry;
}


class CPhrap_Sequence : public CPhrap_Seq
{
public:
    CPhrap_Sequence(const string& name, TPhrapReaderFlags flags);
    virtual void ReadTag(CNcbiIstream& in, char tag);

    // Convert to contig or read depending on the loaded data
    bool IsContig(void) const;
    CRef<CPhrap_Contig> GetContig(void);

    bool IsRead(void) const;
    CRef<CPhrap_Read> GetRead(void);
    void SetRead(CPhrap_Read& read);

private:
    mutable CRef<CPhrap_Seq> m_Seq;
};


CPhrap_Sequence::CPhrap_Sequence(const string& name, TPhrapReaderFlags flags)
    : CPhrap_Seq(name, flags),
      m_Seq(0)
{
    // Check if name ends with '.comp'
    SetComplemented(IsOldComplementedName(name));
    return;
}


void CPhrap_Sequence::ReadTag(CNcbiIstream& in, char tag)
{
    NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: unexpected tag.",
                in.tellg() - CT_POS_TYPE(0));
}


bool CPhrap_Sequence::IsContig(void) const
{
    return m_Seq  &&
        dynamic_cast<const CPhrap_Contig*>(m_Seq.GetPointer()) != 0;
}


CRef<CPhrap_Contig> CPhrap_Sequence::GetContig(void)
{
    if ( !m_Seq ) {
        m_Seq.Reset(new CPhrap_Contig(GetFlags()));
        // Copy existing data into the contig
        m_Seq->CopyFrom(*this);
    }
    _ASSERT( IsContig() );
    return Ref(&dynamic_cast<CPhrap_Contig&>(*m_Seq));
}


bool CPhrap_Sequence::IsRead(void) const
{
    return m_Seq  &&
        dynamic_cast<const CPhrap_Read*>(m_Seq.GetPointer()) != 0;
}


CRef<CPhrap_Read> CPhrap_Sequence::GetRead(void)
{
    if ( !m_Seq ) {
        m_Seq.Reset(new CPhrap_Read(GetName(), GetFlags()));
        // Copy existing data into the read
        m_Seq->CopyFrom(*this);
    }
    _ASSERT( IsRead() );
    return Ref(&dynamic_cast<CPhrap_Read&>(*m_Seq));
}


void CPhrap_Sequence::SetRead(CPhrap_Read& read)
{
    _ASSERT( !m_Seq );
    m_Seq.Reset(CRef<CPhrap_Seq>(&read));
    _ASSERT(GetName() == read.GetName());
    // Copy sequence data, length, pad map etc.
    read.CopyFrom(*this);
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
        ePhrap_WA, // {...}
        ePhrap_WR, // WRong, tag must be ignored

        // Old format tags
        ePhrap_DNA,
        ePhrap_Sequence,
        ePhrap_BaseQuality,
        ePhrap_Assembled_from,
        ePhrap_Assembled_from_Pad,
        ePhrap_Base_segment,
        ePhrap_Base_segment_Pad,
        ePhrap_Clipping,
        ePhrap_Clipping_Pad
    };

    struct SAssmTag
    {
        string          m_Type;
        string          m_Program;
        string          m_Date;
        vector<string>  m_Comments;
    };
    typedef vector<SAssmTag> TAssmTags;

    void x_ConvertContig(void);
    void x_ReadContig(void);
    void x_ReadRead(void);
    void x_ReadTag(const string& tag);  // CT{} and RT{}
    void x_ReadWA(void);                // WA{}
    void x_SkipTag(const string& tag,
                   const string& data); // WR{}, standalone CT{} and RT{}

    void x_ReadOldFormatData(void);     // Read old ACE format data
    void x_ReadOldSequence(CPhrap_Sequence& seq);
    CRef<CPhrap_Contig> x_AddContig(CPhrap_Sequence& seq);
    CRef<CPhrap_Read> x_AddRead(CPhrap_Sequence& seq);

    void x_DetectFormatVersion(void);
    EPhrapTag x_GetTag(void);
    EPhrapTag x_GetNewTag(void); // read new ACE tag (AS, CO etc.)
    EPhrapTag x_GetOldTag(void); // read old ACE tag (Sequence, DNA etc.)

    void x_UngetTag(EPhrapTag tag);

    CPhrap_Seq* x_FindSeq(const string& name);

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
    x_DetectFormatVersion();
    EPhrapTag tag = x_GetTag();
    if ((m_Flags & fPhrap_OldVersion) == 0) {
        // Read new ACE format
        if (tag != ePhrap_AS) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "ReadPhrap: invalid data, AS tag expected.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
        m_Stream >> m_NumContigs >> m_NumReads;
        CheckStreamState(m_Stream, "invalid data in AS tag.");
        for (size_t i = 0; i < m_NumContigs; i++) {
			x_ReadContig();
            x_ConvertContig();
        }
        if (x_GetTag() != ePhrap_eof) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "ReadPhrap: unrecognized extra-data, EOF expected.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
    }
    else {
        // Read old ACE format
        x_UngetTag(tag);
        x_ReadOldFormatData();
    }
	_ASSERT( m_Entry  &&  m_Entry->IsSet() );
    x_CreateDesc(m_Entry->SetSet());

    return m_Entry;
}


void CPhrapReader::x_ConvertContig(void)
{
    if ( m_Contigs.empty() ) {
        return;
    }
    _ASSERT(m_Contigs.size() == 1);
    CRef<CSeq_entry> entry = m_Contigs[0]->CreateContig(
        m_NumContigs > 1 ? 2 : 1);
	m_Contigs.clear();
	m_Seqs.clear();
	if (m_NumContigs == 1) {
		_ASSERT( !m_Entry );
		m_Entry = entry;
	}
	else {
		if ( !m_Entry ) {
			m_Entry.Reset(new CSeq_entry);
			CBioseq_set& bset = m_Entry->SetSet();
			bset.SetLevel(1);
		}
		m_Entry->SetSet().SetSeq_set().push_back(entry);
	}
}


void CPhrapReader::x_UngetTag(EPhrapTag tag)
{
    _ASSERT(m_LastTag == ePhrap_not_set);
    m_LastTag = tag;
}


void CPhrapReader::x_DetectFormatVersion(void)
{
    _ASSERT(m_LastTag == ePhrap_not_set);
    if ((m_Flags & fPhrap_Version) == fPhrap_OldVersion  ||
        (m_Flags & fPhrap_Version) == fPhrap_NewVersion) {
        // Version is forced
        return;
    }
    m_Flags &= ~fPhrap_Version;
    m_Stream >> ws;
    if ( m_Stream.eof() ) {
        return;
    }
    EPhrapTag tag = ePhrap_not_set;
    string str_tag;
    m_Stream >> str_tag;
    if (str_tag == "AS") {
        tag = ePhrap_AS;
    }
    else if (str_tag == "DNA") {
        tag = ePhrap_DNA;
    }
    else if (str_tag == "Sequence") {
        tag = ePhrap_Sequence;
    }
    else if (str_tag == "BaseQuality") {
        tag = ePhrap_BaseQuality;
    }
    if (tag != ePhrap_not_set) {
        x_UngetTag(tag);
        m_Flags |= (tag == ePhrap_AS) ? fPhrap_NewVersion : fPhrap_OldVersion;
        return;
    }
    NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: Can not autodetect ACE format version.",
                m_Stream.tellg() - CT_POS_TYPE(0));
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
    return ((m_Flags & fPhrap_OldVersion) != 0) ?
        x_GetOldTag() : x_GetNewTag();
}


CPhrapReader::EPhrapTag CPhrapReader::x_GetNewTag(void)
{
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
        switch (m_Stream.get()) {
        case 'A':
            return ePhrap_WA;
        case 'R':
            return ePhrap_WR;
        }
        break;
    }
    CheckStreamState(m_Stream, "tag.");
    m_Stream >> ws;
    NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: unknown tag.",
                m_Stream.tellg() - CT_POS_TYPE(0));
    return ePhrap_unknown;
}



CPhrapReader::EPhrapTag CPhrapReader::x_GetOldTag(void)
{
    EPhrapTag tag;
    string str_tag;
    m_Stream >> str_tag;
    if (str_tag == "DNA") {
        tag = ePhrap_DNA;
    }
    else if (str_tag == "Sequence") {
        tag = ePhrap_Sequence;
    }
    else if (str_tag == "BaseQuality") {
        tag = ePhrap_BaseQuality;
    }
    else if (str_tag == "Assembled_from") {
        tag = ePhrap_Assembled_from;
    }
    else if (str_tag == "Assembled_from*") {
        tag = ePhrap_Assembled_from_Pad;
    }
    else if (str_tag == "Base_segment") {
        tag = ePhrap_Base_segment;
    }
    else if (str_tag == "Base_segment*") {
        tag = ePhrap_Base_segment_Pad;
    }
    else if (str_tag == "Clipping") {
        tag = ePhrap_Clipping;
    }
    else if (str_tag == "Clipping*") {
        tag = ePhrap_Clipping_Pad;
    }
    else {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: unknown tag.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    CheckStreamState(m_Stream, "tag.");
    m_Stream >> ws;
    return tag;
}



inline
CPhrap_Seq* CPhrapReader::x_FindSeq(const string& name)
{
    TSeqs::iterator seq = m_Seqs.find(name);
    if (seq == m_Seqs.end()) {
        ERR_POST_X(1, Warning <<
            "Referenced contig or read not found: " << name << ".");
        return 0;
    }
    return &*seq->second;
}


void CPhrapReader::x_ReadTag(const string& tag)
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
    CPhrap_Seq* seq = x_FindSeq(name);
    if ( seq ) {
        seq->ReadTag(m_Stream, tag[0]);
    }
    else {
        x_SkipTag(tag, "{\n" + name + " ");
    }
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


void CPhrapReader::x_SkipTag(const string& tag, const string& data)
{
    m_Stream >> ws;
    string content = data;
    for (string c = NStr::TruncateSpaces(ReadLine(m_Stream));
        c != "}"; c = NStr::TruncateSpaces(ReadLine(m_Stream))) {
        content += c + "\n";
    }
    content += "}";
    CheckStreamState(m_Stream, tag + "{} data.");
    ERR_POST_X(2, Warning << "Skipping tag:\n" << tag << content);
    m_Stream >> ws;
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
    for (tag = x_GetTag(); tag != ePhrap_eof; tag = x_GetTag()) {
        switch ( tag ) {
        case ePhrap_BQ:
            contig->ReadBaseQualities(m_Stream);
            continue;
        case ePhrap_AF:
            contig->ReadReadLocation(m_Stream, m_Seqs);
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
            x_ReadRead();
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
        case ePhrap_WR:
            x_SkipTag("WR", kEmptyStr);
            continue;
        case ePhrap_eof:
            return;
        default:
            x_UngetTag(tag);
        }
        break;
    }
}


void CPhrapReader::x_ReadRead(void)
{
    string read_name;
    m_Stream >> read_name;
    CRef<CPhrap_Read> read;
    {{
        CRef<CPhrap_Seq> seq = m_Seqs[read_name];
        if ( !seq ) {
            read.Reset(new CPhrap_Read(read_name, m_Flags));
            m_Seqs[read_name].Reset(read.GetPointer());
        }
        else {
            read.Reset(dynamic_cast<CPhrap_Read*>(seq.GetPointer()));
        }
    }}
    _ASSERT( read );
    read->Read(m_Stream);
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


CRef<CPhrap_Contig> CPhrapReader::x_AddContig(CPhrap_Sequence& seq)
{
    if ( seq.IsRead() ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: sequence type redifinition for " +
                    seq.GetName() + " - was 'read'.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    // If have a loaded contig, convert it first
    x_ConvertContig();
    // Contig can not be already registered
    CRef<CPhrap_Contig> contig = seq.GetContig();
    m_Contigs.push_back(contig);
    m_Seqs[contig->GetName()] = CRef<CPhrap_Seq>(contig.GetPointer());
    _ASSERT(contig);
    return contig;
}


CRef<CPhrap_Read> CPhrapReader::x_AddRead(CPhrap_Sequence& seq)
{
    if ( seq.IsContig() ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadPhrap: sequence type redifinition for " +
                    seq.GetName() + " - was 'contig'.",
                    m_Stream.tellg() - CT_POS_TYPE(0));
    }
    CRef<CPhrap_Read> read;
    TSeqs::iterator it = m_Seqs.find(seq.GetName());
    if ( it != m_Seqs.end() ) {
        // Read is already registered
        read.Reset(dynamic_cast<CPhrap_Read*>(it->second.GetPointer()));
        if ( !read ) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "ReadPhrap: sequence type redifinition for " +
                        seq.GetName() + " - was 'contig'.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
        seq.SetRead(*read);
    }
    else {
        read = seq.GetRead();
        m_Seqs[read->GetName()] = CRef<CPhrap_Seq>(read.GetPointer());
    }
    _ASSERT(read);
    return read;
}


void CPhrapReader::x_ReadOldFormatData(void)
{
    typedef map<string, CRef<CPhrap_Sequence> > TSequences;
    TSequences seqs;
    CRef<CPhrap_Sequence> seq;
    for (EPhrapTag tag = x_GetTag(); tag != ePhrap_eof; tag = x_GetTag()) {
        string seq_name;
        m_Stream >> seq_name;
        // Check if we have a new sequence
        if ( !seq  ||  seq->GetName() != seq_name ) {
            TSequences::iterator seq_it = seqs.find(seq_name);
            if (seq_it != seqs.end()) {
                seq = seq_it->second;
            }
            else {
                seq.Reset(new CPhrap_Sequence(seq_name, m_Flags));
                seqs[seq_name] = seq;
            }
        }
        switch ( tag ) {
        case ePhrap_DNA:
            seq->ReadData(m_Stream);
            break;
        case ePhrap_Sequence:
            x_ReadOldSequence(*seq);
            break;
        case ePhrap_BaseQuality:
            // BaseQuality tag is defined only for contigs
            x_AddContig(*seq)->ReadBaseQualities(m_Stream);
            break;
        case ePhrap_eof:
            continue;
        default:
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: unexpected tag.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
    }
    x_ConvertContig();
}


void CPhrapReader::x_ReadOldSequence(CPhrap_Sequence& seq)
{
    CRef<CPhrap_Contig> contig;
    if ( seq.IsContig() ) {
        contig = seq.GetContig();
    }
    CRef<CPhrap_Read> read;
    if ( seq.IsRead() ) {
        read = seq.GetRead();
    }
    for (EPhrapTag tag = x_GetTag(); tag != ePhrap_eof; tag = x_GetTag()) {
        // Assembled_from[*] name start stop
        // Base_segment[*] c_start c_stop name r_start r_stop
        // Clipping[*] start stop
        switch ( tag ) {
        case ePhrap_Assembled_from:
        case ePhrap_Base_segment:
        case ePhrap_Clipping:
            // Ignore unpadded coordinates, use only padded versions
            ReadLine(m_Stream);
            continue;
        case ePhrap_Assembled_from_Pad:
            if ( !contig ) {
                contig = x_AddContig(seq);
            }
            contig->ReadReadLocation(m_Stream, m_Seqs);
            break;
        case ePhrap_Base_segment_Pad:
            if ( !contig ) {
                contig = x_AddContig(seq);
            }
            contig->ReadBaseSegment(m_Stream);
            break;
        case ePhrap_Clipping_Pad:
            if ( !read ) {
                read = x_AddRead(seq);
            }
            read->ReadQuality(m_Stream);
            break;
        case ePhrap_DNA:
        case ePhrap_Sequence:
        case ePhrap_BaseQuality:
            // Unget tag and return
            x_UngetTag(tag);
        case ePhrap_eof:
            return;
        default:
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: unexpected tag.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
        if ( read  &&  contig ) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "ReadPhrap: sequence type redifinition.",
                        m_Stream.tellg() - CT_POS_TYPE(0));
        }
    }
}


void CPhrapReader::x_CreateDesc(CBioseq_set& bioseq_set) const
{
    if ( ( (m_Flags & fPhrap_Descr) == 0)  ||  m_AssmTags.empty() ) {
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
