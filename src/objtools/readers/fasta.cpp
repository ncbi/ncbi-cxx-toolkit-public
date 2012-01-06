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
* Authors:  Aaron Ucko, NCBI
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   src/objmgr/util/sequence.cpp.)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/fasta.hpp>
#include "fasta_aln_builder.hpp"
#include <objtools/readers/fasta_exception.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/source_mod_parser.hpp>
#include <objtools/error_codes.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/format_guess.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <ctype.h>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Fasta

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DEF(bool, READ_FASTA, USE_NEW_IMPLEMENTATION, true);

static
CRef<CSeq_entry> s_ReadFasta_OLD(CNcbiIstream& in, TReadFastaFlags flags,
                                 int* counter,
                                 vector<CConstRef<CSeq_loc> >* lcv);

template <typename TStack>
class CTempPusher
{
public:
    typedef typename TStack::value_type TValue;
    CTempPusher(TStack& s, const TValue& v) : m_Stack(s) { s.push(v); }
    ~CTempPusher() { _ASSERT( !m_Stack.empty() );  m_Stack.pop(); }    

private:
    TStack& m_Stack;
};

typedef CTempPusher<stack<CFastaReader::TFlags> > CFlagGuard;

// The FASTA reader uses these heavily, but the standard versions
// aren't inlined on as many configurations as one might hope, and we
// don't necessarily want locale-dependent behavior anyway.

inline bool s_ASCII_IsUpper(unsigned char c)
{
    return c >= 'A'  &&  c <= 'Z';
}

inline bool s_ASCII_IsLower(unsigned char c)
{
    return c >= 'a'  &&  c <= 'z';
}

inline bool s_ASCII_IsAlpha(unsigned char c)
{
    return s_ASCII_IsUpper(c)  ||  s_ASCII_IsLower(c);
}

inline unsigned char s_ASCII_ToUpper(unsigned char c)
{
    return s_ASCII_IsLower(c) ? c + 'A' - 'a' : c;
}

CFastaReader::CFastaReader(ILineReader& reader, TFlags flags)
    : m_LineReader(&reader), m_MaskVec(0), m_IDGenerator(new CSeqIdGenerator)
{
    m_Flags.push(flags);
}

CFastaReader::CFastaReader(CNcbiIstream& in, TFlags flags)
    : m_LineReader(ILineReader::New(in)), m_MaskVec(0),
      m_IDGenerator(new CSeqIdGenerator)
{
    m_Flags.push(flags);
}

CFastaReader::CFastaReader(const string& path, TFlags flags)
    : m_LineReader(ILineReader::New(path)), m_MaskVec(0),
      m_IDGenerator(new CSeqIdGenerator)
{
    m_Flags.push(flags);
}

CFastaReader::~CFastaReader(void)
{
    _ASSERT(m_Flags.size() == 1);
}

CRef<CSeq_entry> CFastaReader::ReadOneSeq(void)
{
    m_CurrentSeq.Reset(new CBioseq);
    // m_CurrentMask.Reset();
    m_SeqData.erase();
    m_Gaps.clear();
    m_CurrentPos = 0;
    m_MaskRangeStart = kInvalidSeqPos;
    if ( !TestFlag(fInSegSet) ) {
        if (m_MaskVec  &&  m_NextMask.IsNull()) {
            m_MaskVec->push_back(SaveMask());
        }
        m_CurrentMask.Reset(m_NextMask);
        if (m_CurrentMask) {
            m_CurrentMask->SetNull();
        }
        m_NextMask.Reset();
        m_SegmentBase = 0;
        m_Offset = 0;
    }
    m_CurrentGapLength = m_TotalGapLength = 0;

    bool need_defline = true;
    while ( !GetLineReader().AtEOF() ) {
        char c = GetLineReader().PeekChar();
        if (GetLineReader().AtEOF()) {
            NCBI_THROW2(CObjReaderParseException, eEOF,
                        "CFastaReader: Input stream no longer valid",
                        LineNumber());
        }
        if (c == '>') {
            if (need_defline) {
                ParseDefLine(*++GetLineReader());
                need_defline = false;
                continue;
            } else {
                if (TestFlag(fParseGaps)) {
                    CTempString next_line = *++GetLineReader();
                    if (next_line.size() > 2  &&  next_line[1] == '?'
                        &&  ParseGapLine(next_line) ) {
                        continue;
                    } else {
                        GetLineReader().UngetLine();
                    }
                }
                // start of the next sequence
                break;
            }
        } else if (c == '[') {
            return x_ReadSegSet();
        } else if (c == ']') {
            if (need_defline) {
                NCBI_THROW2(CObjReaderParseException, eEOF,
                            "CFastaReader: Reached end of segmented set",
                            LineNumber());
            } else {
                break;
            }
        }

        CTempString line = NStr::TruncateSpaces(*++GetLineReader());
        if (line.empty()) {
            continue; // ignore lines containing only whitespace
        }
        c = line[0];

        if (c == '!'  ||  c == '#') {
            // no content, just a comment or blank line
            continue;
        } else if (need_defline) {
            if (TestFlag(fDLOptional)) {
                ParseDefLine(">");
                need_defline = false;
            } else {
                GetLineReader().UngetLine();
                NCBI_THROW2(CObjReaderParseException, eNoDefline,
                            "CFastaReader: Input doesn't start with"
                            " a defline or comment",
                            LineNumber());
            }
        }

        if ( !TestFlag(fNoSeqData) ) {
            ParseDataLine(line);
        }
    }

    if (need_defline  &&  GetLineReader().AtEOF()) {
        NCBI_THROW2(CObjReaderParseException, eEOF,
                    "CFastaReader: Reached end of input",
                    LineNumber());
    }

    AssembleSeq();
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*m_CurrentSeq);

    if(TestFlag(fAddMods)) {
        entry->Parentize();
        x_RecursiveApplyAllMods( *entry );
    }

    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSegSet(void)
{
    CFlagGuard guard(m_Flags, GetFlags() | fInSegSet);
    CRef<CSeq_entry> entry(new CSeq_entry), master(new CSeq_entry), parts;

    _ASSERT(GetLineReader().PeekChar() == '[');
    try {
        ++GetLineReader();
        parts = ReadSet();
    } catch (CObjReaderParseException&) {
        if (GetLineReader().AtEOF()) {
            throw;
        } else if (GetLineReader().PeekChar() == ']') {
            ++GetLineReader();
        } else {
            throw;
        }
    }
    if (GetLineReader().AtEOF()) {
        NCBI_THROW2(CObjReaderParseException, eBadSegSet,
                    "CFastaReader: Segmented set not properly terminated",
                    LineNumber());
    } else if (!parts->IsSet()  ||  parts->GetSet().GetSeq_set().empty()) {
        NCBI_THROW2(CObjReaderParseException, eBadSegSet,
                    "CFastaReader: Segmented set contains no sequences",
                    LineNumber());
    }

    const CBioseq& first_seq = parts->GetSet().GetSeq_set().front()->GetSeq();
    CBioseq& master_seq = master->SetSeq();
    CSeq_inst& inst = master_seq.SetInst();
    // XXX - work out less generic ID?
    CRef<CSeq_id> id(SetIDGenerator().GenerateID(true));
    if (m_CurrentMask) {
        m_CurrentMask->SetId(*id);
    }
    master_seq.SetId().push_back(id);
    inst.SetRepr(CSeq_inst::eRepr_seg);
    inst.SetMol(first_seq.GetInst().GetMol());
    inst.SetLength(GetCurrentPos(ePosWithGapsAndSegs));
    CSeg_ext& ext = inst.SetExt().SetSeg();
    ITERATE (CBioseq_set::TSeq_set, it, parts->GetSet().GetSeq_set()) {
        CRef<CSeq_loc>      seg_loc(new CSeq_loc);
        const CBioseq::TId& seg_ids = (*it)->GetSeq().GetId();
        CRef<CSeq_id>       seg_id = FindBestChoice(seg_ids, CSeq_id::BestRank);
        seg_loc->SetWhole(*seg_id);
        ext.Set().push_back(seg_loc);
    }

    parts->SetSet().SetClass(CBioseq_set::eClass_parts);
    entry->SetSet().SetClass(CBioseq_set::eClass_segset);
    entry->SetSet().SetSeq_set().push_back(master);
    entry->SetSet().SetSeq_set().push_back(parts);
    return entry;
}

CRef<CSeq_entry> CFastaReader::ReadSet(int max_seqs)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    if (TestFlag(fOneSeq)) {
        max_seqs = 1;
    }
    for (int i = 0;  i < max_seqs  &&  !GetLineReader().AtEOF();  ++i) {
        try {
            CRef<CSeq_entry> entry2(ReadOneSeq());
            if (max_seqs == 1) {
                return entry2;
            }
            entry->SetSet().SetSeq_set().push_back(entry2);
        } catch (CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            } else {
                throw;
            }
        }
    }
    if(TestFlag(fAddMods)) {
        entry->Parentize();
        x_RecursiveApplyAllMods( *entry );
    }
    if (entry->IsSet()  &&  entry->GetSet().GetSeq_set().size() == 1) {
        return entry->SetSet().SetSeq_set().front();
    } else {
        return entry;
    }
}

CRef<CSeq_loc> CFastaReader::SaveMask(void)
{
    m_NextMask.Reset(new CSeq_loc);
    return m_NextMask;
}

void CFastaReader::SetIDGenerator(CSeqIdGenerator& gen)
{
    m_IDGenerator.Reset(&gen);
}

void CFastaReader::ParseDefLine(const TStr& s)
{
    size_t start = 1, pos, len = s.length(), range_len = 0, title_start;
    TSeqPos range_start, range_end;
    do {
        bool has_id = true;
        if (TestFlag(fNoParseID)) {
            title_start = start;
        } else {
            for (pos = start;  pos < len;  ++pos) {
                if ((unsigned char) s[pos] <= ' ') { // assumes ASCII
                    break;
                }
            }
            range_len = ParseRange(TStr(s.data() + start, pos - start),
                                   range_start, range_end);
            has_id = ParseIDs(TStr(s.data() + start, pos - start - range_len));
            if (has_id  &&  TestFlag(fAllSeqIds)  &&  s[pos] == '\1') {
                start = pos + 1;
                continue;
            }
            title_start = pos + 1;
            // trim leading whitespace from title (is this appropriate?)
            while (title_start < len
                   &&  isspace((unsigned char) s[title_start])) {
                ++title_start;
            }
        }
        for (pos = title_start + 1;  pos < len;  ++pos) {
            if ((unsigned char) s[pos] < ' ') {
                break;
            }
        }
        if ( !has_id ) {
            // no IDs after all, so take the whole line as a title
            // (done now rather than earlier to avoid rescanning)
            title_start = start;
        }
        if (title_start < min(pos, len)) {
            ParseTitle(s.substr(title_start, pos - title_start));
        }
        start = pos + 1;
    } while (TestFlag(fAllSeqIds)  &&  start < len  &&  s[start - 1] == '\1'
             &&  !range_len);

    if (GetIDs().empty()) {
        // No [usable] IDs
        if (TestFlag(fRequireID)) {
            NCBI_THROW2(CObjReaderParseException, eNoIDs,
                        "CFastaReader: Defline lacks a proper ID",
                        LineNumber());
        }
        GenerateID();
    } else if ( !TestFlag(fForceType) ) {
        // Does any ID imply a specific type?
        ITERATE (CBioseq::TId, it, GetIDs()) {
            CSeq_id::EAccessionInfo acc_info = (*it)->IdentifyAccession();
            if (acc_info & CSeq_id::fAcc_nuc) {
                _ASSERT ( !(acc_info & CSeq_id::fAcc_prot) );
                m_CurrentSeq->SetInst().SetMol(CSeq_inst::eMol_na);
                break;
            } else if (acc_info & CSeq_id::fAcc_prot) {
                m_CurrentSeq->SetInst().SetMol(CSeq_inst::eMol_aa);
                break;
            }
            // XXX - verify that other IDs aren't contradictory?
        }
    }

    m_BestID = FindBestChoice(GetIDs(), CSeq_id::BestRank);

    if (range_len) {
#if 1
        // generate a new ID, and record its relation to the given one(s).
        SetIDs().clear();
        GenerateID();
        CRef<CSeq_align> sa(new CSeq_align);
        sa->SetType(CSeq_align::eType_partial); // ?
        sa->SetDim(2);
        CDense_seg& ds = sa->SetSegs().SetDenseg();
        ds.SetNumseg(1);
        ds.SetIds().push_back(GetIDs().front());
        ds.SetIds().push_back(m_BestID);
        ds.SetStarts().push_back(0);
        ds.SetStarts().push_back(range_start);
        if (range_start > range_end) { // negative strand
            ds.SetLens().push_back(range_start + 1 - range_end);
            ds.SetStrands().push_back(eNa_strand_plus);
            ds.SetStrands().push_back(eNa_strand_minus);
        } else {
            ds.SetLens().push_back(range_end + 1 - range_start);
        }
        m_CurrentSeq->SetInst().SetHist().SetAssembly().push_back(sa);
        _ASSERT(m_BestID->IsLocal()  ||  !GetIDs().front()->IsLocal()
                ||  m_CurrentSeq->GetNonLocalId() == &*m_BestID);
        m_BestID = GetIDs().front();
        m_ExpectedEnd = range_end - range_start;
#else
        // somewhat confusing, and arguably incorrect
        if (range_start > 0) {
            SGap gap = { 0, range_start };
            m_Gaps.push_back(gap);
        }
        m_ExpectedEnd = range_end;
#endif   
    }

    if ( !TestFlag(fNoUserObjs) ) {
        // store the raw defline in a User-object for reference
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetUser().SetType().SetStr("CFastaReader");
        desc->SetUser().AddField("DefLine", NStr::PrintableString(s));
        m_CurrentSeq->SetDescr().Set().push_back(desc);
    }

    if (TestFlag(fUniqueIDs)) {
        ITERATE (CBioseq::TId, it, GetIDs()) {
            CSeq_id_Handle h = CSeq_id_Handle::GetHandle(**it);
            if ( !m_IDTracker.insert(h).second ) {
                NCBI_THROW2(CObjReaderParseException, eDuplicateID,
                            "CFastaReader: Seq-id " + h.AsString()
                            + " is a duplicate",
                            LineNumber());
            }
        }
    }
}

bool CFastaReader::ParseIDs(const TStr& s)
{
    CBioseq::TId& ids = SetIDs();
    // CBioseq::TId  old_ids = ids;
    size_t count = 0;
    CSeq_id::TParseFlags flags = CSeq_id::fParse_PartialOK; // be generous
    if (TestFlag(fParseRawID)) {
        flags |= CSeq_id::fParse_RawText;
    }
    try {
        count = CSeq_id::ParseIDs(ids, s, flags);
    } catch (CSeqIdException&) {
        // swap(ids, old_ids);
    }
    if ( !count ) {
        string str(s);
        // Handled this way rather than by passing CSeq_id::fParse_ValidLocal
        // so that subclasses can meaningfully overload IsValidLocalID.
        if (IsValidLocalID(str)) {
            ids.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, str)));
        } else {
            return false;
        }
    }
    return true;
}

size_t CFastaReader::ParseRange(const TStr& s, TSeqPos& start, TSeqPos& end)
{
    bool    on_start = false;
    bool    negative = false;
    TSeqPos mult = 1;
    size_t  pos;
    start = end = 0;
    for (pos = s.length() - 1;  pos > 0;  --pos) {
        unsigned char c = s[pos];
        if (c >= '0'  &&  c <= '9') {
            if (on_start) {
                start += (c - '0') * mult;
            } else {
                end += (c - '0') * mult;
            }
            mult *= 10;
        } else if (c == '-'  &&  !on_start  &&  mult > 1) {
            on_start = true;
            mult = 1;
        } else if (c == ':'  &&  on_start  &&  mult > 1) {
            break;
        } else if (c == 'c'  &&  pos > 0  &&  s[--pos] == ':'
                   &&  on_start  &&  mult > 1) {
            negative = true;
            break;
        } else {
            return 0; // syntax error
        }
    }
    if ((negative ? (end > start) : (start > end))  ||  s[pos] != ':') {
        return 0;
    }
    return s.length() - pos;
}

void CFastaReader::ParseTitle(const TStr& s)
{
    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetTitle().assign(s.data(), s.length());
    m_CurrentSeq->SetDescr().Set().push_back(desc);
}

bool CFastaReader::IsValidLocalID(const string& s)
{
    return CSeq_id::IsValidLocalID(s);
}

void CFastaReader::GenerateID(void)
{
    if (TestFlag(fUniqueIDs)) { // be extra careful
        CRef<CSeq_id> id;
        TIDTracker::const_iterator idt_end = m_IDTracker.end();
        do {
            id = m_IDGenerator->GenerateID(true);
        } while (m_IDTracker.find(CSeq_id_Handle::GetHandle(*id)) != idt_end);
        SetIDs().push_back(id);
    } else {
        SetIDs().push_back(m_IDGenerator->GenerateID(true));
    }
}

void CFastaReader::CheckDataLine(const TStr& s)
{
    // make sure the first data line has at least SOME resemblance to
    // actual sequence data.
    if (TestFlag(fSkipCheck)  ||  ! m_SeqData.empty() ) {
        return;
    }
    size_t good = 0, bad = 0, len = s.length();
    for (size_t pos = 0;  pos < len;  ++pos) {
        unsigned char c = s[pos];
        if (s_ASCII_IsAlpha(c)  ||  c == '-'  ||  c == '*') {
            ++good;
        } else if (isspace(c)  ||  (c >= '0' && c <= '9')) {
            // treat whitespace and digits as neutral
        } else if (c == ';') {
            break; // comment -- ignore rest of line
        } else {
            ++bad;
        }
    }
    if (bad >= good / 3  &&  (len > 3  ||  good == 0  ||  bad > good)) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "CFastaReader: Input not marked as defline or comment, but "
                    "contains too many special characters to be plausible data",
                    LineNumber());
    }
}

void CFastaReader::ParseDataLine(const TStr& s)
{
    CheckDataLine(s);

    size_t len = s.length();
    if (m_SeqData.capacity() < m_SeqData.size() + len) {
        // ensure exponential capacity growth to avoid quadratic runtime
        m_SeqData.reserve(2 * max(m_SeqData.capacity(), len));
    }
    m_SeqData.resize(m_CurrentPos + len);
    for (size_t pos = 0;  pos < len;  ++pos) {
        unsigned char c = s[pos];
        if (c == '-'  &&  TestFlag(fParseGaps)) {
            CloseMask();
            // OpenGap();
            size_t pos2 = pos + 1;
            while (pos2 < len  &&  s[pos2] == '-') {
                ++pos2;
            }
            m_CurrentGapLength += pos2 - pos;
            pos = pos2 - 1;
        } else if (s_ASCII_IsAlpha(c)  ||  c == '-'  ||  c == '*') {
            // Restrict further if specifically expecting nucleotide data?
            CloseGap();
            if (s_ASCII_IsLower(c)) {
                m_SeqData[m_CurrentPos] = s_ASCII_ToUpper(c);
                OpenMask();
            } else {
                m_SeqData[m_CurrentPos] = c;
                CloseMask();
            }
            ++m_CurrentPos;
        } else if (c == ';') {
            // comment -- ignore rest of line
            break;
        } else if ( !isspace(c) ) {
            if (TestFlag(fValidate)) {
                NCBI_THROW2(CBadResiduesException, eBadResidues,
                            string("CFastaReader: Invalid residue ") + s[pos]
                            + " at position " + NStr::UInt8ToString(pos+1), // "+1" because 1-based for user
                                CBadResiduesException::SBadResiduePositions( m_BestID, pos, LineNumber() ) );
            } else {
                ERR_POST_X(1, Warning
                           << "CFastaReader: Ignoring invalid residue " << c
                           << " at line " << LineNumber()
                           << ", position " << pos);
            }
        } 
    }
    m_SeqData.resize(m_CurrentPos);
}

void CFastaReader::x_CloseGap(TSeqPos len)
{
    _ASSERT(len > 0  &&  TestFlag(fParseGaps));
    if (TestFlag(fAligning)) {
        TSeqPos pos = GetCurrentPos(ePosWithGapsAndSegs);
        m_Starts[pos + m_Offset][m_Row] = CFastaAlignmentBuilder::kNoPos;
        m_Offset += len;
        m_Starts[pos + m_Offset][m_Row] = pos;
    } else {
        TSeqPos pos = GetCurrentPos(eRawPos);
        // Special case -- treat a lone hyphen at the end of a line as
        // a gap of unknown length.
        if (len == 1) {
            TSeqPos l = m_SeqData.length();
            if (l == pos  ||  l == pos + (*GetLineReader()).length()) {
                len = 0;
            }
        }
        SGap gap = { pos, len };
        m_Gaps.push_back(gap);
        m_TotalGapLength += len;
        m_CurrentGapLength = 0;
    }
}

void CFastaReader::x_OpenMask(void)
{
    _ASSERT(m_MaskRangeStart == kInvalidSeqPos);
    m_MaskRangeStart = GetCurrentPos(ePosWithGapsAndSegs);
}

void CFastaReader::x_CloseMask(void)
{
    _ASSERT(m_MaskRangeStart != kInvalidSeqPos);
    m_CurrentMask->SetPacked_int().AddInterval
        (GetBestID(), m_MaskRangeStart, GetCurrentPos(ePosWithGapsAndSegs) - 1,
         eNa_strand_plus);
    m_MaskRangeStart = kInvalidSeqPos;
}

bool CFastaReader::ParseGapLine(const TStr& line)
{
    SGap gap = { GetCurrentPos(eRawPos),
                 NStr::StringToUInt(line.substr(2), NStr::fConvErr_NoThrow) };
    if (gap.len > 0  ||  line == ">?unk100") {
        m_Gaps.push_back(gap);
        return true;
    } else {
        return false;
    }
}

void CFastaReader::AssembleSeq(void)
{
    CSeq_inst& inst = m_CurrentSeq->SetInst();

    CloseGap();
    CloseMask();
    if (TestFlag(fInSegSet)) {
        m_SegmentBase += GetCurrentPos(ePosWithGaps);
    }
    AssignMolType();
    CSeq_data::E_Choice format
        = inst.IsAa() ? CSeq_data::e_Ncbieaa : CSeq_data::e_Iupacna;
    if (TestFlag(fValidate)) {
        CSeq_data tmp_data(m_SeqData, format);
        vector<TSeqPos> badIndexes;
        CSeqportUtil::Validate(tmp_data, &badIndexes);
        if ( ! badIndexes.empty() ) {
            NCBI_THROW2(CBadResiduesException, eBadResidues,
                "CFastaReader: Invalid residue(s) in input sequence",
                CBadResiduesException::SBadResiduePositions( m_BestID, badIndexes, LineNumber() ) );
        }
    }


    if (m_Gaps.empty()) {
        _ASSERT(m_TotalGapLength == 0);
        if (m_SeqData.empty()) {
            inst.SetLength(0);
            inst.SetRepr(CSeq_inst::eRepr_virtual);
        } else if (TestFlag(fNoSplit)) {
            inst.SetLength(GetCurrentPos(eRawPos));
            inst.SetRepr(CSeq_inst::eRepr_raw);
            CRef<CSeq_data> data(new CSeq_data(m_SeqData, format));
            CSeqportUtil::Pack(data, inst.GetLength());
            inst.SetSeq_data(*data);
        } else {
            inst.SetLength(GetCurrentPos(eRawPos));
            CDelta_ext& delta_ext = inst.SetExt().SetDelta();
            delta_ext.AddAndSplit(m_SeqData, format, inst.GetLength(),
                                  TestFlag(fLetterGaps));
            if (delta_ext.Get().size() > 1) {
                inst.SetRepr(CSeq_inst::eRepr_delta);
            } else { // simplify -- just one piece
                inst.SetRepr(CSeq_inst::eRepr_raw);
                inst.SetSeq_data(delta_ext.Set().front()
                                 ->SetLiteral().SetSeq_data());
                inst.ResetExt();
            }
        }
    } else {
        CDelta_ext& delta_ext = inst.SetExt().SetDelta();
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.SetLength(GetCurrentPos(ePosWithGaps));
        SIZE_TYPE n = m_Gaps.size();
        for (SIZE_TYPE i = 0;  i < n;  ++i) {
            if (i == 0  &&  m_Gaps[i].pos > 0) {
                TStr chunk(m_SeqData, 0, m_Gaps[i].pos);
                if (TestFlag(fNoSplit)) {
                    delta_ext.AddLiteral(chunk, inst.GetMol());
                } else {
                    delta_ext.AddAndSplit(chunk, format, m_Gaps[i].pos,
                                          TestFlag(fLetterGaps));
                }
            }

            if (m_Gaps[i].len == 0) { // unknown length
                CRef<CDelta_seq> gap_ds(new CDelta_seq);
                gap_ds->SetLoc().SetNull();
                delta_ext.Set().push_back(gap_ds);
            } else {
                delta_ext.AddLiteral(m_Gaps[i].len);
            }

            TSeqPos next_start = (i == n-1) ? m_CurrentPos : m_Gaps[i+1].pos;
            if (next_start != m_Gaps[i].pos) {
                TSeqPos seq_len = next_start - m_Gaps[i].pos;
                TStr chunk(m_SeqData, m_Gaps[i].pos, seq_len);
                if (TestFlag(fNoSplit)) {
                    delta_ext.AddLiteral(chunk, inst.GetMol());
                } else {
                    delta_ext.AddAndSplit(chunk, format, seq_len,
                                          TestFlag(fLetterGaps));
                }
            }
        }
    }
}

void CFastaReader::AssignMolType(void)
{
    CSeq_inst&                  inst = m_CurrentSeq->SetInst();
    CSeq_inst::EMol             default_mol;
    CFormatGuess::ESTStrictness strictness;

    // Check flags; in general, treat contradictory settings as canceling out.
    // Did the user specify a (default) type?
    switch (GetFlags() & (fAssumeNuc | fAssumeProt)) {
    case fAssumeNuc:   default_mol = CSeq_inst::eMol_na;      break;
    case fAssumeProt:  default_mol = CSeq_inst::eMol_aa;      break;
    default:           default_mol = CSeq_inst::eMol_not_set; break;
    }
    // Did the user request non-default format-guessing strictness?
    switch (GetFlags() & (fStrictGuess | fLaxGuess)) {
    case fStrictGuess:  strictness = CFormatGuess::eST_Strict;  break;
    case fLaxGuess:     strictness = CFormatGuess::eST_Lax;     break;
    default:            strictness = CFormatGuess::eST_Default; break;
    }

    if (TestFlag(fForceType)) {
        _ASSERT(default_mol != CSeq_inst::eMol_not_set);
        inst.SetMol(default_mol);
        return;
    } else if (inst.IsSetMol()) {
        return; // previously found an informative ID
    } else if (m_SeqData.empty()) {
        // Nothing else to go on, but that's OK (no sequence to worry
        // about encoding); however, Seq-inst.mol is still mandatory.
        inst.SetMol(CSeq_inst::eMol_not_set);
        return;
    }

    // Do the residue frequencies suggest a specific type?
    SIZE_TYPE length = min(m_SeqData.length(), SIZE_TYPE(4096));
    switch (CFormatGuess::SequenceType(m_SeqData.data(), length, strictness)) {
    case CFormatGuess::eNucleotide:  inst.SetMol(CSeq_inst::eMol_na);  return;
    case CFormatGuess::eProtein:     inst.SetMol(CSeq_inst::eMol_aa);  return;
    default:
        if (default_mol == CSeq_inst::eMol_not_set) {
            NCBI_THROW2(CObjReaderParseException, eAmbiguous,
                        "CFastaReader: Unable to determine sequence type",
                        LineNumber());
        } else {
            inst.SetMol(default_mol);
        }
    }
}

// XXX - no longer called
void CFastaReader::SaveSeqData(CSeq_data& seq_data, const TStr& raw_string)
{
    SIZE_TYPE len = raw_string.length();
    if (m_CurrentSeq->IsAa()) {
        seq_data.SetNcbieaa().Set().assign(raw_string.data(),
                                           len);
    } else {
        // nucleotide -- pack to ncbi2na, or at least ncbi4na
        vector<char> v((len+1) / 2, '\0');
        CSeqUtil::ECoding coding;
        CSeqConvert::Pack(raw_string.data(), len, CSeqUtil::e_Iupacna, &v[0],
                          coding);
        if (coding == CSeqUtil::e_Ncbi2na) {
            seq_data.SetNcbi2na().Set().assign(v.begin(),
                                               v.begin() + (len + 3) / 4);
        } else {
            swap(seq_data.SetNcbi4na().Set(), v);
        }
    }
}

CRef<CSeq_entry> CFastaReader::ReadAlignedSet(int reference_row)
{
    TIds             ids;
    CRef<CSeq_entry> entry = x_ReadSeqsToAlign(ids);
    CRef<CSeq_annot> annot(new CSeq_annot);

    if ( !entry->IsSet()
        ||  entry->GetSet().GetSeq_set().size() < max(reference_row + 1, 2)) {
        NCBI_THROW2(CObjReaderParseException, eEOF,
                    "CFastaReader::ReadAlignedSet: not enough input sequences.",
                    LineNumber());
    } else if (reference_row >= 0) {
        x_AddPairwiseAlignments(*annot, ids, reference_row);
    } else {
        x_AddMultiwayAlignment(*annot, ids);
    }
    entry->SetSet().SetAnnot().push_back(annot);

    if(TestFlag(fAddMods)) {
        entry->Parentize();
        x_RecursiveApplyAllMods( *entry );
    }

    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSeqsToAlign(TIds& ids)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    vector<TSeqPos>  lengths;

    CFlagGuard guard(m_Flags, GetFlags() | fAligning | fParseGaps);

    for (m_Row = 0, m_Starts.clear();  !GetLineReader().AtEOF();  ++m_Row) {
        try {
            // must mark m_Starts prior to reading in case of leading gaps
            m_Starts[0][m_Row] = 0;
            CRef<CSeq_entry> entry2(ReadOneSeq());
            entry->SetSet().SetSeq_set().push_back(entry2);
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(GetBestID());
            ids.push_back(id);
            lengths.push_back(GetCurrentPos(ePosWithGapsAndSegs) + m_Offset);
            _ASSERT(lengths.size() == size_t(m_Row) + 1);
            // redundant if there was a trailing gap, but that should be okay
            m_Starts[lengths[m_Row]][m_Row] = CFastaAlignmentBuilder::kNoPos;
        } catch (CObjReaderParseException&) {
            if (GetLineReader().AtEOF()) {
                break;
            } else {
                throw;
            }
        }
    }
    // check whether lengths are all equal, and warn if they differ
    if (lengths.size() > 1 && TestFlag(fValidate)) {
        vector<TSeqPos>::const_iterator it(lengths.begin());
        const TSeqPos len = *it;
        for (++it; it != lengths.end(); ++it) {
            if (*it != len) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            "CFastaReader::ReadAlignedSet: Rows have different "
                            "lengths", LineNumber());
            }
        }
    }

    return entry;
}

void CFastaReader::x_AddPairwiseAlignments(CSeq_annot& annot, const TIds& ids,
                                           TRowNum reference_row)
{
    typedef CFastaAlignmentBuilder TBuilder;
    typedef CRef<TBuilder>         TBuilderRef;

    TRowNum             rows = m_Row;
    vector<TBuilderRef> builders(rows);
    
    for (TRowNum r = 0;  r < rows;  ++r) {
        if (r != reference_row) {
            builders[r].Reset(new TBuilder(ids[reference_row], ids[r]));
        }
    }
    ITERATE (TStartsMap, it, m_Starts) {
        const TSubMap& submap = it->second;
        TSubMap::const_iterator rr_it2 = submap.find(reference_row);
        if (rr_it2 == submap.end()) { // reference unchanged
            ITERATE (TSubMap, it2, submap) {
                int r = it2->first;
                _ASSERT(r != reference_row);
                builders[r]->AddData(it->first, TBuilder::kContinued,
                                     it2->second);
            }
        } else { // reference changed; all rows need updating
            TSubMap::const_iterator it2 = submap.begin();
            for (TRowNum r = 0;  r < rows;  ++r) {
                if (it2 != submap.end()  &&  r == it2->first) {
                    if (r != reference_row) {
                        builders[r]->AddData(it->first, rr_it2->second,
                                             it2->second);
                    }
                    ++it2;
                } else {
                    _ASSERT(r != reference_row);
                    builders[r]->AddData(it->first, rr_it2->second,
                                         TBuilder::kContinued);
                }
            }
        }
    }

    // finalize and store the alignments
    CSeq_annot::TData::TAlign& annot_align = annot.SetData().SetAlign();
    for (TRowNum r = 0;  r < rows;  ++r) {
        if (r != reference_row) {
            annot_align.push_back(builders[r]->GetCompletedAlignment());
        }
    }
}

void CFastaReader::x_AddMultiwayAlignment(CSeq_annot& annot, const TIds& ids)
{
    TRowNum              rows = m_Row;
    CRef<CSeq_align>     sa(new CSeq_align);
    CDense_seg&          ds   = sa->SetSegs().SetDenseg();
    CDense_seg::TStarts& dss  = ds.SetStarts();

    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(rows);
    ds.SetDim(rows);
    ds.SetIds() = ids;
    dss.reserve((m_Starts.size() - 1) * rows);

    TSeqPos old_len = 0;
    for (TStartsMap::const_iterator next = m_Starts.begin(), it = next++;
         next != m_Starts.end();  it = next++) {
        TSeqPos len = next->first - it->first;
        _ASSERT(len > 0);
        ds.SetLens().push_back(len);

        const TSubMap&          submap = it->second;
        TSubMap::const_iterator it2 = submap.begin();
        for (TRowNum r = 0;  r < rows;  ++r) {
            if (it2 != submap.end()  &&  r == it2->first) {
                dss.push_back(it2->second);
                ++it2;
            } else {
                _ASSERT(dss.size() >= size_t(rows)  &&  old_len > 0);
                TSignedSeqPos last_pos = dss[dss.size() - rows];
                if (last_pos == CFastaAlignmentBuilder::kNoPos) {
                    dss.push_back(last_pos);
                } else {
                    dss.push_back(last_pos + old_len);
                }
            }
        }

        it = next;
        old_len = len;
    }
    ds.SetNumseg(ds.GetLens().size());
    annot.SetData().SetAlign().push_back(sa);
}


CRef<CSeq_id> CSeqIdGenerator::GenerateID(bool advance)
{
    CRef<CSeq_id> seq_id(new CSeq_id);
    int n = advance ? m_Counter.Add(1) - 1 : m_Counter.Get();
    if (m_Prefix.empty()  &&  m_Suffix.empty()) {
        seq_id->SetLocal().SetId(n);
    } else {
        string& id = seq_id->SetLocal().SetStr();
        id.reserve(128);
        id += m_Prefix;
        id += NStr::IntToString(n);
        id += m_Suffix;
    }
    return seq_id;
}

CRef<CSeq_id> CSeqIdGenerator::GenerateID(void) const
{
    return const_cast<CSeqIdGenerator*>(this)->GenerateID(false);
}


class CCounterManager
{
public:
    CCounterManager(CSeqIdGenerator& generator, int* counter)
        : m_Generator(generator), m_Counter(counter)
        { if (counter) { generator.SetCounter(*counter); } }
    ~CCounterManager()
        { if (m_Counter) { *m_Counter = m_Generator.GetCounter(); } }

private:
    CSeqIdGenerator& m_Generator;
    int*             m_Counter;
};

CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags,
                           int* counter, vector<CConstRef<CSeq_loc> >* lcv)
{
    typedef NCBI_PARAM_TYPE(READ_FASTA, USE_NEW_IMPLEMENTATION) TParam_NewImpl;

    TParam_NewImpl new_impl;

    if (new_impl.Get()) {
        CRef<ILineReader> lr(ILineReader::New(in));
        CFastaReader      reader(*lr, flags);
        CCounterManager   counter_manager(reader.SetIDGenerator(), counter);
        if (lcv) {
            reader.SaveMasks(reinterpret_cast<CFastaReader::TMasks*>(lcv));
        }
        return reader.ReadSet();
    } else {
        return s_ReadFasta_OLD(in, flags, counter, lcv);
    }
}


IFastaEntryScan::~IFastaEntryScan()
{
}


class CFastaMapper : public CFastaReader
{
public:
    typedef CFastaReader TParent;

    CFastaMapper(ILineReader& reader, SFastaFileMap* fasta_map, TFlags flags);

protected:
    void ParseDefLine(const TStr& s);
    void ParseTitle(const TStr& s);
    void AssembleSeq(void);

private:
    SFastaFileMap*             m_Map;
    SFastaFileMap::SFastaEntry m_MapEntry;
};

CFastaMapper::CFastaMapper(ILineReader& reader, SFastaFileMap* fasta_map,
                           TFlags flags)
    : TParent(reader, flags), m_Map(fasta_map)
{
    _ASSERT(fasta_map);
    fasta_map->file_map.resize(0);
}

void CFastaMapper::ParseDefLine(const TStr& s)
{
    TParent::ParseDefLine(s); // We still want the default behavior.
    m_MapEntry.seq_id = GetIDs().front()->AsFastaString(); // XXX -- GetBestID?
    m_MapEntry.all_seq_ids.resize(0);
    ITERATE (CBioseq::TId, it, GetIDs()) {
        m_MapEntry.all_seq_ids.push_back((*it)->AsFastaString());
    }
    m_MapEntry.stream_offset = StreamPosition() - s.length();
}

void CFastaMapper::ParseTitle(const TStr& s)
{
    TParent::ParseTitle(s);
    m_MapEntry.description = s;
}

void CFastaMapper::AssembleSeq(void)
{
    TParent::AssembleSeq();
    m_Map->file_map.push_back(m_MapEntry);
}


void ReadFastaFileMap(SFastaFileMap* fasta_map, CNcbiIfstream& input)
{
    static const CFastaReader::TFlags kFlags
        = CFastaReader::fAssumeNuc | CFastaReader::fAllSeqIds
        | CFastaReader::fNoSeqData;

    if ( !input.is_open() ) {
        return;
    }

    CRef<ILineReader> lr(ILineReader::New(input));
    CFastaMapper      mapper(*lr, fasta_map, kFlags);
    mapper.ReadSet();
}


void ScanFastaFile(IFastaEntryScan* scanner, 
                   CNcbiIfstream&   input,
                   TReadFastaFlags  fread_flags)
{
    if ( !input.is_open() ) {
        return;
    }

    CRef<ILineReader> lr(ILineReader::New(input));
    CFastaReader      reader(*lr, fread_flags);

    while ( !lr->AtEOF() ) {
        try {
            CNcbiStreampos   pos = lr->GetPosition();
            CRef<CSeq_entry> se  = reader.ReadOneSeq();
            if (se->IsSeq()) {
                scanner->EntryFound(se, pos);
            }
        } catch (CObjReaderParseException&) {
            if ( !lr->AtEOF() ) {
                throw;
            }
        }
    }
}


/// Everything below this point is specific to the old implementation.

static SIZE_TYPE s_EndOfFastaID(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE vbar = str.find('|', pos);
    if (vbar == NPOS) {
        return NPOS; // bad
    }

    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(str.substr(pos, vbar - pos).c_str());

#if 1
    if (choice != CSeq_id::e_not_set) {
        SIZE_TYPE vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            if (vbar == NPOS) {
                break;
            }
            choice = CSeq_id::WhichInverseSeqId(
                str.substr(vbar_prev + 1, vbar - vbar_prev - 1).c_str());
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return NPOS; // bad
    }
#else
    switch (choice) {
    case CSeq_id::e_Patent: case CSeq_id::e_Other: // 3 args
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Genbank:   case CSeq_id::e_Embl:    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot: case CSeq_id::e_General: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Prf:       case CSeq_id::e_Pdb:     case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:       case CSeq_id::e_Tpd:
        // 2 args
        if (vbar == NPOS) {
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Local: case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:  case CSeq_id::e_Gi:
        // 1 arg
        if (vbar == NPOS) {
            if (choice == CSeq_id::e_Other) {
                // this is acceptable - member is optional
                break;
            }
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        break;

    default: // unrecognized or not set
        return NPOS; // bad
    }
#endif

    return (vbar == NPOS) ? str.size() : vbar;
}


static bool s_IsValidLocalID(const string& s)
{
    static const char* const kLegal =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.:";
    return (!s.empty()  &&  s.find_first_not_of(kLegal) == NPOS);
}


static void s_FixSeqData(CBioseq* seq)
{
    _ASSERT(seq);
    CSeq_inst& inst = seq->SetInst();
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_delta:
    {
        TSeqPos length = 0;
        NON_CONST_ITERATE (CDelta_ext::Tdata, it,
                           inst.SetExt().SetDelta().Set()) {
            if ((*it)->IsLiteral()  &&  (*it)->GetLiteral().IsSetSeq_data()) {
                CSeq_literal& lit  = (*it)->SetLiteral();
                CSeq_data&    data = lit.SetSeq_data();
                if (data.IsIupacna()) {
                    lit.SetLength(data.GetIupacna().Get().size());
                    CSeqportUtil::Pack(&data);
                } else {
                    string& s = data.SetNcbieaa().Set();
                    lit.SetLength(s.size());
                    s.reserve(s.size()); // free extra allocation
                }
                length += lit.GetLength();
            }
        }
        break;
    }
    case CSeq_inst::eRepr_raw:
    {
        CSeq_data& data = inst.SetSeq_data();
        if (data.IsIupacna()) {
            inst.SetLength(data.GetIupacna().Get().size());
            CSeqportUtil::Pack(&data);
        } else {
            string& s = data.SetNcbieaa().Set();
            inst.SetLength(s.size());
            s.reserve(s.size()); // free extra allocation
        }        
        break;
    }
    default: // especially not_set!
        break;
    }
}


static void s_AddData(CSeq_inst& inst, const string& residues)
{
    CRef<CSeq_data> data;
    if (inst.IsSetExt()  &&  inst.GetExt().IsDelta()) {
        CDelta_ext::Tdata& delta_data = inst.SetExt().SetDelta().Set();
        if (delta_data.empty()  ||  !delta_data.back()->IsLiteral()
            ||  !delta_data.back()->GetLiteral().IsSetSeq_data()) {
            CRef<CDelta_seq> delta_seq(new CDelta_seq);
            delta_data.push_back(delta_seq);
            data = &delta_seq->SetLiteral().SetSeq_data();
        } else {
            data = &delta_data.back()->SetLiteral().SetSeq_data();
        }
    } else {
        data = &inst.SetSeq_data();
    }

    string* s = 0;
    if (inst.GetMol() == CSeq_inst::eMol_aa) {
        if (data->IsNcbieaa()) {
            s = &data->SetNcbieaa().Set();
        } else {
            data->SetNcbieaa().Set(residues);
        }
    } else {
        if (data->IsIupacna()) {
            s = &data->SetIupacna().Set();
        } else {
            data->SetIupacna().Set(residues);
        }
    }

    if (s) {
        // grow exponentially to avoid O(n^2) behavior
        if (s->capacity() < s->size() + residues.size()) {
            s->reserve(s->capacity()
                       + max(residues.size(), s->capacity() / 2));
        }
        *s += residues;
    }
}


static CSeq_inst::EMol s_ParseFastaDefline(CBioseq::TId& ids, string& title,
                                           const string& line,
                                           TReadFastaFlags flags, int* counter)
{
    SIZE_TYPE       start = 0;
    CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
    do {
        ++start;
        SIZE_TYPE space = line.find_first_of(" \t", start);
        string    name  = line.substr(start, space - start);
        string    local;

        if (flags & fReadFasta_NoParseID) {
            space = start - 1;
        } else {
            // try to parse out IDs
            SIZE_TYPE pos = 0;
            while (pos < name.size()) {
                SIZE_TYPE end = s_EndOfFastaID(name, pos);
                if (end == NPOS) {
                    if (pos > 0) {
                        NCBI_THROW2(CObjReaderParseException, eFormat,
                                    "s_ParseFastaDefline: Bad ID "
                                    + name.substr(pos),
                                    pos);
                    } else if (s_IsValidLocalID(name)) {
                        local = name;
                    } else {
                        space = start - 1;
                    }
                    break;
                }

                CRef<CSeq_id> id(new CSeq_id(name.substr(pos, end - pos)));
                ids.push_back(id);
                if (mol == CSeq_inst::eMol_not_set
                    &&  !(flags & fReadFasta_ForceType)) {
                    CSeq_id::EAccessionInfo ai = id->IdentifyAccession();
                    if (ai & CSeq_id::fAcc_nuc) {
                        mol = CSeq_inst::eMol_na;
                    } else if (ai & CSeq_id::fAcc_prot) {
                        mol = CSeq_inst::eMol_aa;
                    }
                }
                pos = end + 1;
            }
        }

        if ( !local.empty() ) {
            ids.push_back(CRef<CSeq_id>
                          (new CSeq_id(CSeq_id::e_Local, local, kEmptyStr)));
        }

        start = line.find('\1', start);
        if (space != NPOS  &&  title.empty()) {
            title.assign(line, space + 1,
                         (start == NPOS) ? NPOS : (start - space - 1));
        }
    } while (start != NPOS  &&  (flags & fReadFasta_AllSeqIds));

    if (ids.empty()) {
        if (flags & fReadFasta_RequireID) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "s_ParseFastaDefline: no ID present", 0);
        }
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetId((*counter)++);
        ids.push_back(id);
    }

    return mol;
}


static void s_GuessMol(CSeq_inst::EMol& mol, const string& data,
                       TReadFastaFlags flags, istream& in)
{
    if (mol != CSeq_inst::eMol_not_set) {
        return; // already known; no need to guess
    }

    if (mol == CSeq_inst::eMol_not_set  &&  !(flags & fReadFasta_ForceType)) {
        switch (CFormatGuess::SequenceType(data.data(), data.size())) {
        case CFormatGuess::eNucleotide:  mol = CSeq_inst::eMol_na;  return;
        case CFormatGuess::eProtein:     mol = CSeq_inst::eMol_aa;  return;
        default:                         break;
        }
    }

    // ForceType was set, or CFormatGuess failed, so we have to rely on
    // explicit assumptions
    if (flags & fReadFasta_AssumeNuc) {
        _ASSERT(!(flags & fReadFasta_AssumeProt));
        mol = CSeq_inst::eMol_na;
    } else if (flags & fReadFasta_AssumeProt) {
        mol = CSeq_inst::eMol_aa;
    } else { 
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: unable to deduce molecule type"
                    " from IDs, flags, or sequence",
                    in.tellg() - CT_POS_TYPE(0));
    }
}


static
CRef<CSeq_entry> s_ReadFasta_OLD(CNcbiIstream& in, TReadFastaFlags flags,
                                 int* counter,
                                 vector<CConstRef<CSeq_loc> >* lcv)
{
    if ( !in ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: Input stream no longer valid",
                    in.tellg() - CT_POS_TYPE(0));
    } else {
        CT_INT_TYPE c = in.peek();
        if ( !strchr(">#!\n\r", CT_TO_CHAR_TYPE(c)) ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: Input doesn't start with a defline or comment",
                 in.tellg() - CT_POS_TYPE(0));
        }
    }

    CRef<CSeq_entry>       entry(new CSeq_entry);
    CBioseq_set::TSeq_set& sset  = entry->SetSet().SetSeq_set();
    CRef<CBioseq>          seq(0); // current Bioseq
    string                 line;
    TSeqPos                pos = 0, lc_start = 0;
    bool                   was_lc = false, in_gap = false;
    CRef<CSeq_id>          best_id;
    CRef<CSeq_loc>         lowercase(0);
    int                    defcounter = 1;

    if ( !counter ) {
        counter = &defcounter;
    }

    while ( !in.eof() ) {
        if ((flags & fReadFasta_OneSeq)  &&  seq.NotEmpty()
            &&  (in.peek() == '>')) {
            break;
        }
        NcbiGetlineEOL(in, line);
        if (NStr::EndsWith(line, "\r")) {
            line.resize(line.size() - 1);
        }
        if (in.eof()  &&  line.empty()) {
            break;
        } else if (line.empty()) {
            continue;
        }
        if (line[0] == '>') {
            // new sequence
            if (seq) {
                s_FixSeqData(seq);
                if (was_lc) {
                    lowercase->SetPacked_int().AddInterval
                        (*best_id, lc_start, pos - 1);
                }
            }
            seq = new CBioseq;
            if (flags & fReadFasta_NoSeqData) {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
            } else {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            }
            {{
                CRef<CSeq_entry> entry2(new CSeq_entry);
                entry2->SetSeq(*seq);
                sset.push_back(entry2);
            }}
            string          title;
            CSeq_inst::EMol mol = s_ParseFastaDefline(seq->SetId(), title,
                                                      line, flags, counter);
            if (mol == CSeq_inst::eMol_not_set
                &&  (flags & fReadFasta_NoSeqData)) {
                if (flags & fReadFasta_AssumeNuc) {
                    _ASSERT(!(flags & fReadFasta_AssumeProt));
                    mol = CSeq_inst::eMol_na;
                } else if (flags & fReadFasta_AssumeProt) {
                    mol = CSeq_inst::eMol_aa;
                }
            }
            seq->SetInst().SetMol(mol);

            if ( !title.empty() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetTitle(title);
                seq->SetDescr().Set().push_back(desc);
            }

            if (lcv) {
                pos       = 0;
                was_lc    = false;
                best_id   = FindBestChoice(seq->GetId(), CSeq_id::Score);
                lowercase = new CSeq_loc;
                lowercase->SetNull();
                lcv->push_back(lowercase);
            }
            in_gap = false;
        } else if (line[0] == '#'  ||  line[0] == '!') {
            continue; // comment
        } else if ( !seq ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: No defline preceding data",
                 in.tellg() - CT_POS_TYPE(0));
        } else if ( !(flags & fReadFasta_NoSeqData) ) {
            // These don't change, but the calls may be relatively expensive,
            // esp. with ref-counted implementations.
            SIZE_TYPE   line_size = line.size();
            const char* line_data = line.data();
            // actual data; may contain embedded junk
            CSeq_inst&  inst      = seq->SetInst();
            string      residues(line_size + 1, '\0');
            char*       res_data  = const_cast<char*>(residues.data());
            SIZE_TYPE   res_count = 0;
            for (SIZE_TYPE i = 0;  i < line_size;  ++i) {
                char c = line_data[i];
                if (isalpha((unsigned char) c)) {
                    in_gap = false;
                    if (lowercase) {
                        bool is_lc = islower((unsigned char) c) ? true : false;
                        if (is_lc && !was_lc) {
                            lc_start = pos;
                        } else if (was_lc && !is_lc) {
                            lowercase->SetPacked_int().AddInterval
                                (*best_id, lc_start, pos - 1);
                        }
                        was_lc = is_lc;
                        ++pos;
                    }
                    res_data[res_count++] = toupper((unsigned char) c);
                } else if (c == '-'  &&  (flags & fReadFasta_ParseGaps)) {
                    CDelta_ext::Tdata& d = inst.SetExt().SetDelta().Set();
                    if (in_gap) {
                        ++d.back()->SetLiteral().SetLength();
                        continue; // count long gaps
                    }
                    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
                        CRef<CDelta_seq> ds(new CDelta_seq);
                        inst.SetRepr(CSeq_inst::eRepr_delta);
                        if (inst.IsSetSeq_data()) {
                            ds->SetLiteral().SetSeq_data(inst.SetSeq_data());
                            d.push_back(ds);
                            inst.ResetSeq_data();
                        }
                    }
                    if ( res_count ) {
                        residues.resize(res_count);
                        if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                            s_GuessMol(inst.SetMol(), residues, flags, in);
                        }
                        s_AddData(inst, residues);
                    }
                    in_gap    = true;
                    res_count = 0;
                    CRef<CDelta_seq> gap(new CDelta_seq);
                    if (line.find_first_not_of(" \t\r\n", i + 1) == NPOS) {
                        // consider a single - at the end of a line as
                        // a gap of unknown length, as we sometimes format
                        // them that way
                        gap->SetLoc().SetNull();
                    } else {
                        gap->SetLiteral().SetLength(1);
                    }
                    d.push_back(gap);
                } else if (c == '-'  ||  c == '*') {
                    in_gap = false;
                    // valid, at least for proteins
                    res_data[res_count++] = c;
                } else if (c == ';') {
                    i = line_size;
                    continue; // skip rest of line
                } else if ( !isspace((unsigned char) c) ) {
                    ERR_POST_X(2, Warning << "ReadFasta: Ignoring invalid residue "
                               << c << " at position "
                               << (in.tellg() - CT_POS_TYPE(0)));
                }
            }

            if (res_count) {
                // Add the accumulated data...
                residues.resize(res_count);
                if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                    s_GuessMol(inst.SetMol(), residues, flags, in);
                }            
                s_AddData(inst, residues);
            }
        }
    }

    if (seq) {
        s_FixSeqData(seq);
        if (was_lc) {
            lowercase->SetPacked_int().AddInterval(*best_id, lc_start, pos - 1);
        }
    }
    // simplify if possible
    if (sset.size() == 1) {
        entry->SetSeq(*seq);
    }
    return entry;
}


void CFastaReader::x_RecursiveApplyAllMods( CSeq_entry& entry )
{
    if (entry.IsSet()) {
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it,
                           entry.SetSet().SetSeq_set()) {
            x_RecursiveApplyAllMods(**it);
        }
    } else {
        CBioseq&         seq = entry.SetSeq();
        CSourceModParser smp( TestFlag(fBadModThrow) ?
            CSourceModParser::eHandleBadMod_Throw : 
            CSourceModParser::eHandleBadMod_Ignore );
        CConstRef<CSeqdesc> title_desc
            = seq.GetClosestDescriptor(CSeqdesc::e_Title);
        if (title_desc) {
            string& title(const_cast<string&>(title_desc->GetTitle()));
            title = smp.ParseTitle(title);
            smp.ApplyAllMods(seq);
            if( TestFlag(fUnknModThrow) ) {
                CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
                if( ! unused_mods.empty() ) 
                {
                    // there are unused mods and user specified to throw if any
                    // unused 
                    CNcbiOstrstream err;
                    err << "CFastaReader: Unused mods on ";

                    // get sequence ID
                    const CSeq_id* seq_id = seq.GetFirstId();
                    if( seq_id ) {
                        err << seq_id->GetSeqIdString();
                    } else {
                        // seq-id unknown
                        err << "sequence";
                    }

                    err << ":";
                    ITERATE(CSourceModParser::TMods, mod_iter, unused_mods) {
                        err << " [" << mod_iter->key << "=" << mod_iter->value << ']';
                    }
                    NCBI_THROW2(CObjReaderParseException, eUnusedMods,
                        (string)CNcbiOstrstreamToString(err),
                        LineNumber());
                }
            }
            smp.GetLabel(&title, CSourceModParser::fUnusedMods);
            copy( smp.GetBadMods().begin(), smp.GetBadMods().end(),
                inserter(m_BadMods, m_BadMods.begin()) );
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
