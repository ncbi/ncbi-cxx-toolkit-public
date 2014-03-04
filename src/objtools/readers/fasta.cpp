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

#include <objects/misc/sequence_macros.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
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

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/line_error.hpp>

#include <ctype.h>

#define FASTA_LINE_EXPT(_eSeverity, _uLineNum, _MessageStrmOps,  _eErrCode, _eProblem, _sFeature, _sQualName, _sQualValue) \
    do {                                                                \
        const string sSeqId_49518053 = ( m_BestID ?                     \
                                         m_BestID->AsFastaString() :    \
                                         kEmptyStr  );                  \
        const size_t uLineNum_49518053 = (_uLineNum);                   \
        stringstream err_strm_49518053;                                 \
        err_strm_49518053 << _MessageStrmOps;                           \
        AutoPtr<CObjReaderLineException> pLineExpt(                        \
            CObjReaderLineException::Create(                            \
                (_eSeverity), uLineNum_49518053,                        \
                err_strm_49518053.str(),                                \
                (_eProblem),                                            \
                sSeqId_49518053, (_sFeature),                           \
                (_sQualName), (_sQualValue),                            \
                CObjReaderParseException::_eErrCode) );                 \
        if ( ! pMessageListener && (_eSeverity) <= eDiag_Warning ) {    \
            ERR_POST_X(1, "FASTA-Reader: Warning: " + pLineExpt->Message()); \
        } else if ( ! pMessageListener || ! pMessageListener->PutError( *pLineExpt ) ) \
        {                                                               \
            NCBI_THROW2(CObjReaderParseException, _eErrCode,            \
                        err_strm_49518053.str(), uLineNum_49518053 );   \
        }                                                               \
    } while(0)

#define FASTA_PROGRESS(_MessageStrmOps)                                 \
    do {                                                                \
        stringstream err_strm_49518053;                                 \
        err_strm_49518053 << _MessageStrmOps;                           \
        if( pMessageListener ) {                                        \
            pMessageListener->PutProgress(err_strm_49518053.str());     \
        }                                                               \
    } while(false)


#define FASTA_WARNING(_uLineNum, _MessageStrmOps, _eProblem, _Feature) \
    FASTA_LINE_EXPT(eDiag_Warning, _uLineNum, _MessageStrmOps, eFormat, _eProblem, _Feature, kEmptyStr, kEmptyStr)

#define FASTA_WARNING_EX(_uLineNum, _MessageStrmOps, _eProblem, _Feature, _sQualName, _sQualValue) \
    FASTA_LINE_EXPT(eDiag_Warning, _uLineNum, _MessageStrmOps, eFormat, _eProblem, _Feature, _sQualName, _sQualValue)

#define FASTA_ERROR(_uLineNum, _MessageStrmOps, _eErrCode) \
    FASTA_LINE_EXPT(eDiag_Error, _uLineNum, _MessageStrmOps, _eErrCode, ILineError::eProblem_GeneralParsingError, kEmptyStr, kEmptyStr, kEmptyStr)

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Fasta

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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

// temporarily swap two CRef's, then swap them back again on destruction
// (RAII)
template<class TObject>
class CTempRefSwap {
public:
    CTempRefSwap(
        CRef<TObject> & pObj1,
        CRef<TObject> & pObj2 ) :
    m_pObj1(pObj1),
        m_pObj2(pObj2) 
    { 
        m_pObj1.Swap(m_pObj2);
    }

    ~CTempRefSwap(void)
    {
        // swap back when done
        m_pObj1.Swap(m_pObj2);
    }

private:
    CRef<TObject> & m_pObj1;
    CRef<TObject> & m_pObj2;
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

inline bool s_ASCII_IsAmbigNuc(unsigned char c)
{
    switch( s_ASCII_ToUpper(c) ) {
    case 'U':
    case 'R':
    case 'Y':
    case 'S':
    case 'W':
    case 'K':
    case 'M':
    case 'B':
    case 'D':
    case 'H':
    case 'V':
    case 'N':
        return true;
    default:
        return false;
    }
}

inline bool s_ASCII_IsUnAmbigNuc(unsigned char c)
{
    switch( c ) {
    case 'A':
    case 'C':
    case 'G':
    case 'T':
    case 'a':
    case 'c':
    case 'g':
    case 't':
        return true;
    default:
        return false;
    }
}

inline bool s_ASCII_IsValidNuc(unsigned char c)
{
    switch( s_ASCII_ToUpper(c) ) {
    case 'A':
    case 'C':
    case 'G':
    case 'T':
    case 'U':
    case 'R':
    case 'Y':
    case 'S':
    case 'W':
    case 'K':
    case 'M':
    case 'B':
    case 'D':
    case 'H':
    case 'V':
    case 'N':
        return true;
    default:
        return false;
    }
}

CFastaReader::CFastaReader(ILineReader& reader, TFlags flags)
    : m_LineReader(&reader), m_MaskVec(0), 
      m_IDGenerator(new CSeqIdGenerator), m_MaxIDLength(kMax_UI4)
{
    m_Flags.push(flags);
}

CFastaReader::CFastaReader(CNcbiIstream& in, TFlags flags)
    : m_LineReader(ILineReader::New(in)), m_MaskVec(0),
      m_IDGenerator(new CSeqIdGenerator), m_MaxIDLength(kMax_UI4)
{
    m_Flags.push(flags);
}

CFastaReader::CFastaReader(const string& path, TFlags flags)
    : m_LineReader(ILineReader::New(path)), m_MaskVec(0),
      m_IDGenerator(new CSeqIdGenerator), m_MaxIDLength(kMax_UI4)
{
    m_Flags.push(flags);
}

CFastaReader::CFastaReader(CReaderBase::TReaderFlags fBaseFlags, TFlags flags)
    : CReaderBase(fBaseFlags), m_MaskVec(0), 
      m_IDGenerator(new CSeqIdGenerator), m_MaxIDLength(kMax_UI4)
{
    m_Flags.push(flags);
}

CFastaReader::~CFastaReader(void)
{
    _ASSERT(m_Flags.size() == 1);
}

CRef<CSerialObject>
CFastaReader::ReadObject (ILineReader &lr, IMessageListener *pMessageListener)
{
    CRef<CSerialObject> object( 
        ReadSeqEntry( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}

CRef<CSeq_entry> 
CFastaReader::ReadSeqEntry (ILineReader &lr, IMessageListener *pMessageListener)
{
    CRef<ILineReader> pTempLineReader( &lr );
    CTempRefSwap<ILineReader> tempRefSwap(m_LineReader, pTempLineReader);

    return ReadSet(kMax_Int, pMessageListener);
}

CRef<CSeq_entry> CFastaReader::ReadOneSeq(IMessageListener * pMessageListener)
{
    m_CurrentSeq.Reset(new CBioseq);
    // m_CurrentMask.Reset();
    m_SeqData.erase();
    m_Gaps.clear();
    m_CurrentPos = 0;
    m_BestID.Reset();
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
    m_CurrentGapChar = '\0';
    m_CurrentSeqTitles.clear();

    bool need_defline = true;
    CBadResiduesException::SBadResiduePositions bad_residue_positions;
    while ( !GetLineReader().AtEOF() ) {
        char c = GetLineReader().PeekChar();
        if( LineNumber() % 10000 == 0 && LineNumber() != 0 ) {
            FASTA_PROGRESS("Processing line " << LineNumber());
        }
        if (GetLineReader().AtEOF()) {
            FASTA_ERROR(LineNumber(), 
                        "CFastaReader: Unexpected end-of-file around line " << LineNumber(),
                        eEOF );
            break;
        }
        if (c == '>' ) {
            CTempString next_line = *++GetLineReader();
            if( NStr::StartsWith(next_line, ">?") ) {
                // This is actually a data line. an assembly gap, in particular, which
                // we handle farther below
                GetLineReader().UngetLine();
            } else {
                if(need_defline) {
                    ParseDefLine(next_line, pMessageListener);
                    need_defline = false;
                    continue;
                } else {
                    GetLineReader().UngetLine();
                    // start of the next sequence
                    break;
                }
            }
        } else if (c == '[') {
            return x_ReadSegSet(pMessageListener);
        } else if (c == ']') {
            if (need_defline) {
                FASTA_ERROR(LineNumber(), 
                    "CFastaReader: Reached unexpected end of segmented set around line " << LineNumber(),
                    eEOF );
            }
            break;
        }

        CTempString line = NStr::TruncateSpaces_Unsafe(*++GetLineReader());
        if (line.empty()) {
            continue; // ignore lines containing only whitespace
        }
        c = line[0];

        if (c == '!'  ||  c == '#') {
            // no content, just a comment or blank line
            continue;
        } else if (need_defline) {
            if (TestFlag(fDLOptional)) {
                ParseDefLine(">", pMessageListener);
                need_defline = false;
            } else {
                GetLineReader().UngetLine();
                NCBI_THROW2(CObjReaderParseException, eNoDefline, 
                            "CFastaReader: Input doesn't start with"
                            " a defline or comment around line " + NStr::NumericToString(LineNumber()),
                             LineNumber() );
            }
        }

        if ( !TestFlag(fNoSeqData) ) {
            try {
                ParseDataLine(line, pMessageListener);
            } catch(CBadResiduesException & e) {
                // we have to catch this exception so we can build up
                // information on all lines, not just the first line
                // with a bad residue
                bad_residue_positions.m_SeqId = e.GetBadResiduePositions().m_SeqId;
                bad_residue_positions.AddBadIndexMap(e.GetBadResiduePositions().m_BadIndexMap);
            }
        }
    }

    if( ! bad_residue_positions.m_BadIndexMap.empty() ) {
        // bad residues unconditionally throws, for now.
        // (not worth the refactoring at this time)
        NCBI_THROW2(CBadResiduesException, eBadResidues,
            "CFastaReader: There are invalid " + x_NucOrProt() + "residue(s) in input sequence",
            bad_residue_positions );
    }

    if (need_defline  &&  GetLineReader().AtEOF()) {
        FASTA_ERROR(LineNumber(),
            "CFastaReader: Expected defline around line " << LineNumber(),
            eEOF);
    }

    AssembleSeq(pMessageListener);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*m_CurrentSeq);

    entry->Parentize();
    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSegSet(IMessageListener * pMessageListener)
{
    CFlagGuard guard(m_Flags, GetFlags() | fInSegSet);
    CRef<CSeq_entry> entry(new CSeq_entry), master(new CSeq_entry), parts;

    _ASSERT(GetLineReader().PeekChar() == '[');
    try {
        ++GetLineReader();
        parts = ReadSet(kMax_Int, pMessageListener);
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
                    "CFastaReader: Segmented set not properly terminated around line " + NStr::NumericToString(LineNumber()),
                    LineNumber());
    } else if (!parts->IsSet()  ||  parts->GetSet().GetSeq_set().empty()) {
        NCBI_THROW2(CObjReaderParseException, eBadSegSet,
                    "CFastaReader: Segmented set contains no sequences around line " + NStr::NumericToString(LineNumber()),
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

CRef<CSeq_entry> CFastaReader::ReadSet(int max_seqs, IMessageListener * pMessageListener)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    if (TestFlag(fOneSeq)) {
        max_seqs = 1;
    }
    for (int i = 0;  i < max_seqs  &&  !GetLineReader().AtEOF();  ++i) {
        try {
            CRef<CSeq_entry> entry2(ReadOneSeq(pMessageListener));
            if (max_seqs == 1) {
                return entry2;
            }
            if (entry2.NotEmpty())
              entry->SetSet().SetSeq_set().push_back(entry2);
        } catch (CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            } else {
                throw;
            }
        }
    }

    entry->Parentize();

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

void CFastaReader::ParseDefLine(const TStr& s, IMessageListener * pMessageListener)
{
    size_t start = 1, pos, len = s.length(), range_len = 0, title_start;
    TSeqPos range_start, range_end;

    // ignore spaces between '>' and the sequence ID
    for( ; start < len; ++start ) {
        if( ! isspace(s[start]) ) {
            break;
        }
    }

    do {
        bool has_id = true;
        if (TestFlag(fNoParseID)) {
            title_start = start;
        } else {
            // This loop finds the end of the sequence ID
            for ( pos = start;  pos < len;  ++pos) {
                unsigned char c = s[pos];
                
                if (c <= ' ' ) { // assumes ASCII
                    break;
                } else if( c == '[' ) {

                    // see if this is part of a FASTA mod, which
                    // implies a pattern like "[key=value]".  We only check
                    // that it looks *roughly* like a FASTA mod and only before the '='
                    //
                    // It might be worth it to put the body of this "if" into its own function,
                    // for clarity if nothing else.

                    const size_t left_bracket_pos = pos;
                    ++pos;

                    // arbitrary, but shouldn't be too much bigger than the largest possible mod key for efficiency
                    const static size_t kMaxCharsToLookAt = 30; 
                    // we give up much sooner than the length of the string, if the string is long.
                    // also note that we give up *before* the end so even if pos
                    // reaches bracket_give_up_pos, we can still say s[pos] without worrying
                    // about array-out-of-bounds issues.
                    const size_t bracket_give_up_pos = min(len - 1, kMaxCharsToLookAt);
                    // keep track of the first space we find, because that becomes the end of the seqid
                    // if this turns out not to be a FASTA mod.
                    size_t first_space_pos = kMax_UI4;

                    // find the end of the key
                    for( ; pos < bracket_give_up_pos ; ++pos ) {
                        const unsigned char c = s[pos];
                        if( c == '=' ) {
                            break;
                        } else if( c <= ' ' ) {
                            first_space_pos = min(first_space_pos, pos);
                            // keep going
                        } else if( isalnum(c) || c == '-' || c == '_' ) {
                            // this is fine; keep going
                        } else {
                            // bad character, so this is NOT a FASTA mod
                            break;
                        }
                    }

                    if( s[pos] == '=' ) {
                        // this seems to be a FASTA mod, so consider the left square bracket
                        // to be the end of the seqid
                        pos = left_bracket_pos;
                        break;
                    } else {
                        // if we stopped on anything but an equal sign, this is NOT a 
                        // FASTA mod.
                        if( first_space_pos < len ) {
                            // If we've found a space at any point, we consider that the end of the seq-id
                            pos = first_space_pos;
                            break;
                        }

                        // it's not a FASTA mod and we didn't find any spaces, so just
                        // keep going as normal, continuing from where we let off at "pos"
                    }
                }
            }

            range_len = ParseRange(TStr(s.data() + start, pos - start),
                                   range_start, range_end, pMessageListener);
            has_id = ParseIDs(TStr(s.data() + start, pos - start - range_len), pMessageListener);
            if (has_id  &&  TestFlag(fAllSeqIds)  &&  s[pos] == '\1') {
                start = pos + 1;
                continue;
            }
            title_start = pos;
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
            // we parse the titles after we know what molecule this is
            m_CurrentSeqTitles.push_back(
                SLineTextAndLoc(
                s.substr(title_start, pos - title_start), LineNumber()));
        }
        start = pos + 1;
    } while (TestFlag(fAllSeqIds)  &&  start < len  &&  s[start - 1] == '\1'
             &&  !range_len);

    if (GetIDs().empty()) {
        // No [usable] IDs
        if (TestFlag(fRequireID)) {
            FASTA_ERROR(LineNumber(),
                        "CFastaReader: Defline lacks a proper ID around line " << LineNumber(),
                        eNoIDs );
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
        // generate a new ID, and record its relation to the given one(s).
        SetIDs().clear();
        GenerateID();
        CRef<CSeq_align> sa(new CSeq_align);
        sa->SetType(CSeq_align::eType_partial); // ?
        sa->SetDim(2);
        CDense_seg& ds = sa->SetSegs().SetDenseg();
        ds.SetNumseg(1);
        ds.SetDim(2); // redundant, but required by validator
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
    }

    FASTA_PROGRESS("Processing Seq-id: " <<
        ( m_BestID ? m_BestID->AsFastaString() : string("UNKNOWN") ) );

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
                FASTA_ERROR(LineNumber(),
                    "CFastaReader: Seq-id " << h.AsString()
                    << " is a duplicate around line " << LineNumber(),
                    eDuplicateID );
            }
        }
    }
}

bool CFastaReader::ParseIDs(
    const TStr& s, IMessageListener * pMessageListener)
{
    // if user wants all ids to be purely local, no problem
    if( m_iFlags & CReaderBase::fAllIdsAsLocal )
    {
        return new CSeq_id(CSeq_id::e_Local, s);
    }
    CBioseq::TId& ids = SetIDs();
    // CBioseq::TId  old_ids = ids;
    size_t count = 0;
    // be generous overall, and give raw local IDs the benefit of the
    // doubt for now
    CSeq_id::TParseFlags flags
        = CSeq_id::fParse_PartialOK | CSeq_id::fParse_AnyLocal;
    if (TestFlag(fParseRawID)) {
        flags |= CSeq_id::fParse_RawText;
    }

    try {
        if (s.find(',') != TStr::npos && s.find('|') == TStr::npos)
        {
            string temp = NStr::Replace(s, ",", "_");
            count = CSeq_id::ParseIDs(ids, temp, flags);
            FASTA_WARNING(LineNumber(),
                "CFastaReader: Near line " << LineNumber() 
                << ", the sequence contains 'comma' symbol and replaced with 'underscore' "
                << "symbol. Please find and correct the sequence id.",
                ILineError::eProblem_GeneralParsingError, "");
        }
        else
        {
            count = CSeq_id::ParseIDs(ids, s, flags);
        }
    } catch (CSeqIdException&) {
        // swap(ids, old_ids);
    }

    // numerics become local, if requested
    if( m_iFlags & CReaderBase::fNumericIdsAsLocal ) {
        NON_CONST_ITERATE(CBioseq::TId, id_it, ids) {
            CSeq_id & id = **id_it;
            if( id.IsGi() ) {
                const TGi gi = id.GetGi();
                id.SetLocal().SetStr( NStr::NumericToString(gi) );
            }
        }
    }
    // recheck raw local IDs
    if (count == 1  &&  ids.back()->IsLocal())
    {
        string temp;
        ids.back()->GetLabel(&temp, 0, CSeq_id::eContent);
        if (!IsValidLocalID(temp))
        {
            //&&  !NStr::StartsWith(s, "lcl|", NStr::eNocase)
            ids.clear();
            return false;
        }
    }
    // check if ID was too long
    if( count == 1 && s.length() > m_MaxIDLength ){ 

        // before throwing an ID-too-long error, check if what we
        // think is a "sequence ID" is actually sequence data
        CMessageListenerLenient dummy_err_container; // so we can ignore them
        if( CreateWarningsForSeqDataInTitle(s, LineNumber(), &dummy_err_container) ) {
            // it's actually seq data
            return false;
        }

        FASTA_ERROR(LineNumber(),
            "CFastaReader: Near line " << LineNumber() 
            << ", the sequence ID is too long.  Its length is " << s.length()
            << " but the max length allowed is " << m_MaxIDLength 
            << ".  Please find and correct all sequence IDs that are too long.",
            eIDTooLong);
    }
    return count > 0;
}

size_t CFastaReader::ParseRange(
    const TStr& s, TSeqPos& start, TSeqPos& end, 
    IMessageListener * pMessageListener)
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
    --start;
    --end;
    return s.length() - pos;
}

void CFastaReader::ParseTitle(
    const SLineTextAndLoc & lineInfo, IMessageListener * pMessageListener)
{
    const static size_t kWarnTitleLength = 1000;
    if( lineInfo.m_sLineText.length() > kWarnTitleLength ) {
        FASTA_WARNING(lineInfo.m_iLineNum,
            "FASTA-Reader: Title is very long: " << lineInfo.m_sLineText.length()
            << " characters (max is " << kWarnTitleLength << ")",
            ILineError::eProblem_TooLong, "defline");
    }

    CreateWarningsForSeqDataInTitle(lineInfo.m_sLineText,lineInfo.m_iLineNum, pMessageListener);

    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetTitle().assign(
        lineInfo.m_sLineText.data(), lineInfo.m_sLineText.length());
    m_CurrentSeq->SetDescr().Set().push_back(desc);

    x_ApplyAllMods(*m_CurrentSeq, lineInfo.m_iLineNum, pMessageListener);
}

bool CFastaReader::IsValidLocalID(const TStr& s)
{
    if (TestFlag(fQuickIDCheck)) { // just check first character
        return CSeq_id::IsValidLocalID(s.substr(0, 1));
    } else {
        return CSeq_id::IsValidLocalID(s);
    }
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

void CFastaReader::CheckDataLine(
    const TStr& s, IMessageListener * pMessageListener)
{
    // make sure the first data line has at least SOME resemblance to
    // actual sequence data.
    if (TestFlag(fSkipCheck)  ||  ! m_SeqData.empty() ) {
        return;
    }
    const bool bIgnoreHyphens = TestFlag(fHyphensIgnoreAndWarn);
    size_t good = 0, bad = 0, len = s.length();
    const bool bIsNuc = (
        ( TestFlag(fAssumeNuc) && TestFlag(fForceType) ) ||
        ( m_CurrentSeq && m_CurrentSeq->IsSetInst() &&
          m_CurrentSeq->GetInst().IsSetMol() &&  m_CurrentSeq->IsNa() ) );
    size_t ambig_nuc = 0;
    for (size_t pos = 0;  pos < len;  ++pos) {
        unsigned char c = s[pos];
        if (s_ASCII_IsAlpha(c) ||  c == '*') {
            ++good;
            if( bIsNuc && s_ASCII_IsAmbigNuc(c) ) {
                ++ambig_nuc;
            }
        } else if( c == '-' ) {
            if( ! bIgnoreHyphens ) {
                ++good;
            }
            // if bIgnoreHyphens == true, the "hyphens are ignored" warning
            // will be triggered elsewhere
        } else if (isspace(c)  ||  (c >= '0' && c <= '9')) {
            // treat whitespace and digits as neutral
        } else if (c == ';') {
            break; // comment -- ignore rest of line
        } else {
            ++bad;
        }
    }
    if (bad >= good / 3  &&  (len > 3  ||  good == 0  ||  bad > good)) {
        FASTA_ERROR( LineNumber(),
            "CFastaReader: Near line " << LineNumber()
            << ", there's a line that doesn't look like plausible data, "
            "but it's not marked as defline or comment.",
            eFormat);
    }
    // warn if more than a certain percentage is ambiguous nucleotides
    const static size_t kWarnPercentAmbiguous = 40; // e.g. "40" means "40%"
    const size_t percent_ambig = (ambig_nuc * 100) / good;
    if( len > 3 && percent_ambig > kWarnPercentAmbiguous ) {
        FASTA_WARNING(LineNumber(), 
            "FASTA-Reader: First data line in seq is about "
            << percent_ambig << "% ambiguous nucleotides (shouldn't be over "
            << kWarnPercentAmbiguous << "%)",   
            ILineError::eProblem_TooManyAmbiguousResidues,
            "first data line");
    }
}

void CFastaReader::ParseDataLine(
    const TStr& s, IMessageListener * pMessageListener)
{
    if( NStr::StartsWith(s, ">?") ) {
        ParseGapLine(s, pMessageListener);
        return;
    }

    CheckDataLine(s, pMessageListener);

    size_t len = min(s.length(), s.find(';')); // ignore ;-delimited comments
    if (m_SeqData.capacity() < m_SeqData.size() + len) {
        // ensure exponential capacity growth to avoid quadratic runtime
        m_SeqData.reserve(2 * max(m_SeqData.capacity(), len));
    }
    if ((GetFlags() & (fSkipCheck | fParseGaps | fValidate)) == fSkipCheck
        &&  m_CurrentMask.Empty()) {
        m_SeqData.append(s.data(), len);
        m_CurrentPos += len;
        return;
    }

    // we're stricter with nucs, so try to determine if we should
    // assume this is a nuc
    bool bIsNuc = false;
    if( ! TestFlag(fForceType) &&
        FIELD_CHAIN_OF_2_IS_SET(*m_CurrentSeq, Inst, Mol) ) 
    {
        if( m_CurrentSeq->IsNa() ) {
            bIsNuc = true;
        }
    } else {
        bIsNuc = TestFlag(fAssumeNuc);
    }
        
    m_SeqData.resize(m_CurrentPos + len);

    // these will stay as -1 and empty unless there's an error
    int bad_pos_line_num = -1;
    vector<TSeqPos> bad_pos_vec; 

    const bool bHyphensIgnoreAndWarn = TestFlag(fHyphensIgnoreAndWarn);
    const bool bHyphensAreGaps = 
        ( TestFlag(fParseGaps) && ! bHyphensIgnoreAndWarn );
    const bool bAllowLetterGaps =
        ( TestFlag(fParseGaps) && TestFlag(fLetterGaps) );

    bool bIgnorableHyphenSeen = false;

    const char chLetterGap = ( bIsNuc ? 'N' : 'X');
    for (size_t pos = 0;  pos < len;  ++pos) {
        unsigned char c = s[pos];
        if ( ( (c == '-')         && bHyphensAreGaps) ||
             ( (c == chLetterGap) && bAllowLetterGaps) )
        {
            CloseMask();
            // open a gap
            size_t pos2 = pos + 1;
            while (pos2 < len  &&  s[pos2] == c) {
                ++pos2;
            }
            m_CurrentGapLength += pos2 - pos;
            m_CurrentGapChar = c;
            pos = pos2 - 1;
        } else if( c == '-' && bHyphensIgnoreAndWarn ) {
            bIgnorableHyphenSeen = true;
        } else if (
            s_ASCII_IsValidNuc(c) ||
            ( ! bIsNuc &&  ( s_ASCII_IsAlpha(c) ||  c == '*' ) ) ||
            c == '-' ) 
        {
            CloseGap(pos == 0);
            if (s_ASCII_IsLower(c)) {
                m_SeqData[m_CurrentPos] = s_ASCII_ToUpper(c);
                OpenMask();
            } else {
                m_SeqData[m_CurrentPos] = c;
                CloseMask();
            }
            ++m_CurrentPos;
        } else if ( !isspace(c) ) {
            if( bad_pos_line_num < 0 ) {
                bad_pos_line_num = LineNumber();
            }
            bad_pos_vec.push_back(pos);
        }
    }

    m_SeqData.resize(m_CurrentPos);

    if( bIgnorableHyphenSeen ) {
        _ASSERT( bHyphensIgnoreAndWarn );
        FASTA_WARNING_EX(LineNumber(),
            "CFastaReader: Hyphens are invalid and will be ignored around line " << LineNumber(),
            ILineError::eProblem_IgnoredResidue,
            kEmptyStr, kEmptyStr, "-" );
    }

    // before throwing, be sure that we're in a valid state so that callers can
    // parse multiple lines and get the invalid residues in all of them.
    
    if( ! bad_pos_vec.empty() ) {
        if (TestFlag(fValidate)) {
            NCBI_THROW2(CBadResiduesException, eBadResidues,
                "CFastaReader: There are invalid " + x_NucOrProt() + "residue(s) in input sequence",
                CBadResiduesException::SBadResiduePositions( m_BestID, bad_pos_vec, bad_pos_line_num ) );
        } else {
            stringstream warn_strm;
            warn_strm << "FASTA-Reader: Ignoring invalid " << x_NucOrProt() 
                << "residues at position(s): ";
            CBadResiduesException::SBadResiduePositions( 
                m_BestID, bad_pos_vec, bad_pos_line_num ).ConvertBadIndexesToString(warn_strm);

            FASTA_WARNING(0, 
                warn_strm.str(),
                ILineError::eProblem_InvalidResidue,
                kEmptyStr );
        }
    }
}

void CFastaReader::x_CloseGap(
    TSeqPos len, bool atStartOfLine, IMessageListener * pMessageListener)
{
    _ASSERT(len > 0  &&  TestFlag(fParseGaps));
    if (TestFlag(fAligning)) {
        TSeqPos pos = GetCurrentPos(ePosWithGapsAndSegs);
        m_Starts[pos + m_Offset][m_Row] = CFastaAlignmentBuilder::kNoPos;
        m_Offset += len;
        m_Starts[pos + m_Offset][m_Row] = pos;
    } else {
        TSeqPos pos = GetCurrentPos(eRawPos);
        SGap::EKnownSize eKnownSize = SGap::eKnownSize_Yes;
        // Special case -- treat a lone hyphen at the end of a line as
        // a gap of unknown length.
        // (do NOT treat a lone 'N' or 'X' as unknown length)
        if (len == 1 && m_CurrentGapChar == '-' ) {
            TSeqPos l = m_SeqData.length();
            if ((l == pos  ||  l == pos + (*GetLineReader()).length())  && atStartOfLine) {
                //and it's not the first col of the line
                len = 0;
                eKnownSize = SGap::eKnownSize_No;
            }
        }
        TGapRef pGap( new SGap(
            pos, len,
            eKnownSize,
            LineNumber()) );

        m_Gaps.push_back(pGap);
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

bool CFastaReader::ParseGapLine(
    const TStr& line, IMessageListener * pMessageListener)
{
    _ASSERT( NStr::StartsWith(line, ">?") );

    // just in case there's a gap before this one,
    // even though somewhat unusual
    CloseGap();

    // sRemainingLine will hold the part of the line left to parse
    TStr sRemainingLine = line.substr(2);
    NStr::TruncateSpacesInPlace(sRemainingLine);

    const TSeqPos uPos = GetCurrentPos(eRawPos);

    // check if size is unknown
    SGap::EKnownSize eIsKnown = SGap::eKnownSize_Yes;
    if( NStr::StartsWith(sRemainingLine, "unk") ) {
        eIsKnown = SGap::eKnownSize_No;
        sRemainingLine = sRemainingLine.substr(3);
        NStr::TruncateSpacesInPlace(sRemainingLine, NStr::eTrunc_Begin);
    }

    // extract the gap size
    TSeqPos uGapSize = 0;
    {
        // find how many digits in number
        TStr::size_type uNumDigits = 0;
        while( uNumDigits != sRemainingLine.size() &&
               ::isdigit(sRemainingLine[uNumDigits]) )
        {
            ++uNumDigits;
        }
        TStr sDigits = sRemainingLine.substr(
            0, uNumDigits);
        uGapSize = NStr::StringToUInt(sDigits, NStr::fConvErr_NoThrow);
        if( uGapSize <= 0 ) {
            FASTA_WARNING(LineNumber(), 
                "CFastaReader: Bad gap size at line " << LineNumber(),
                ILineError::eProblem_NonPositiveLength,
                "gapline" );
            // try to continue the best we can
            uGapSize = 1;
        }
        sRemainingLine = sRemainingLine.substr(sDigits.length());
        NStr::TruncateSpacesInPlace(sRemainingLine, NStr::eTrunc_Begin);
    }

    // extract the raw key-value pairs for the gap
    typedef multimap<TStr, TStr> TModKeyValueMultiMap;
    TModKeyValueMultiMap modKeyValueMultiMap;
    while( ! sRemainingLine.empty() ) {
        TStr::size_type uOpenBracketPos = TStr::npos;
        if ( NStr::StartsWith(sRemainingLine, "[") ) {
            uOpenBracketPos = 0;
        }
        TStr::size_type uPosOfEqualSign = TStr::npos;
        if( uOpenBracketPos != TStr::npos ) {
            // uses "1" to skip the '['
            uPosOfEqualSign = sRemainingLine.find('=', uOpenBracketPos + 1);
        }
        TStr::size_type uCloseBracketPos = TStr::npos;
        if( uPosOfEqualSign != TStr::npos ) {
            uCloseBracketPos = sRemainingLine.find(']', uPosOfEqualSign + 1);
        }
        if( uCloseBracketPos == TStr::npos ) 
        {
            FASTA_WARNING(LineNumber(),
                "CFastaReader: Problem parsing gap mods at line " 
                << LineNumber(),
                ILineError::eProblem_ParsingModifiers,
                "gapline" );
            break; // give up on mod-parsing
        }

        // extract the key and the value
        TStr sKey = NStr::TruncateSpaces_Unsafe(
            sRemainingLine.substr(uOpenBracketPos + 1, 
                (uPosOfEqualSign - uOpenBracketPos - 1) ) );
        TStr sValue = NStr::TruncateSpaces_Unsafe(
            sRemainingLine.substr(uPosOfEqualSign + 1,
                uCloseBracketPos - uPosOfEqualSign - 1) );
        
        // remember what we saw
        modKeyValueMultiMap.insert(
            TModKeyValueMultiMap::value_type(sKey, sValue) );

        // prepare for the next loop around
        sRemainingLine = sRemainingLine.substr(uCloseBracketPos + 1);
        NStr::TruncateSpacesInPlace(sRemainingLine, NStr::eTrunc_Begin);
    }

    // string to value maps
    const CEnumeratedTypeValues::TNameToValue & linkage_evidence_to_value_map =
        CLinkage_evidence::GetTypeInfo_enum_EType()->NameToValue();

    // remember if there is a gap-type conflict
    bool bConflictingGapTypes = false;
    // extract the mods, if any
    SGap::TNullableGapType pGapType;
    ELinkEvid eLinkEvid = eLinkEvid_UnspecifiedOnly;
    set<CLinkage_evidence::EType> setOfLinkageEvidence;
    ITERATE(TModKeyValueMultiMap, modKeyValue_it, modKeyValueMultiMap) {
        const TStr & sKey   = modKeyValue_it->first;
        const TStr & sValue = modKeyValue_it->second;

        string canonicalKey = CanonicalizeString(sKey);
        if(  canonicalKey == "gap-type") {

            const SGapTypeInfo *pGapTypeInfo = NameToGapTypeInfo(sValue);
            if( pGapTypeInfo ) {
                 CSeq_gap::EType eGapType = pGapTypeInfo->m_eType;

                if( ! pGapType ) {
                    pGapType.Reset( new SGap::TGapTypeObj(eGapType) );
                    eLinkEvid = pGapTypeInfo->m_eLinkEvid;
                } else if( eGapType != *pGapType ) {
                    // check if pGapType already set and different
                    bConflictingGapTypes = true;
                }
            } else {
                FASTA_WARNING_EX(
                    LineNumber(),
                    "Unknown gap-type: " << sValue,
                    ILineError::eProblem_ParsingModifiers,
                    "gapline",
                    "gap-type",
                    sValue );
            }

        } else if( canonicalKey == "linkage-evidence") {

            // could be semi-colon separated
            vector<CTempString> arrLinkageEvidences;
            NStr::Tokenize(sValue, ";", arrLinkageEvidences, 
                NStr::eMergeDelims);

            ITERATE(vector<CTempString>, link_evid_it, arrLinkageEvidences) {
                CTempString sLinkEvid = *link_evid_it;
                CEnumeratedTypeValues::TNameToValue::const_iterator find_iter =
                    linkage_evidence_to_value_map.find(CanonicalizeString(sLinkEvid));
                if( find_iter != linkage_evidence_to_value_map.end() ) {
                    setOfLinkageEvidence.insert(
                        static_cast<CLinkage_evidence::EType>(
                        find_iter->second));
                } else {
                    FASTA_WARNING_EX(
                        LineNumber(),
                        "Unknown linkage-evidence: " << sValue,
                        ILineError::eProblem_ParsingModifiers,
                        "gapline",
                        "linkage-evidence",
                        sValue );
                }
            }

        } else {
            // unknown mod.
            FASTA_WARNING_EX(
                LineNumber(),
                "Unknown gap modifier name(s): " << sKey,
                ILineError::eProblem_UnrecognizedQualifierName,
                "gapline", sKey, kEmptyStr );
        }
    }

    if( bConflictingGapTypes ) {
        FASTA_WARNING_EX(LineNumber(),
            "There were conflicting gap-types around line " << LineNumber(),
            ILineError::eProblem_ContradictoryModifiers,
            "gapline", "gap-type", kEmptyStr );
    }

    // check validation beyond basic parsing problems

    // if no gap-type set (but linkage-evidence explicitly set, use "unknown")
    if( ! pGapType && ! setOfLinkageEvidence.empty() ) {
        pGapType.Reset( new SGap::TGapTypeObj(CSeq_gap::eType_unknown) );
    }

    // check if linkage-evidence(s) compatible with gap-type
    switch( eLinkEvid ) {
    case eLinkEvid_UnspecifiedOnly:
        if( setOfLinkageEvidence.empty() ) {
            if( pGapType ) {
                // silently add the required "unspecified"
                setOfLinkageEvidence.insert(CLinkage_evidence::eType_unspecified);
            }
        } else if( setOfLinkageEvidence.size() > 1 || 
            *setOfLinkageEvidence.begin() != CLinkage_evidence::eType_unspecified )
        {
            // only "unspecified" is allowed
            FASTA_WARNING(
                LineNumber(), 
                "FASTA-Reader: Unknown gap-type can have linkage-evidence "
                    "of type 'unspecified' only.",
                ILineError::eProblem_ExtraModifierFound,
                "gapline");
            setOfLinkageEvidence.clear();
            setOfLinkageEvidence.insert(CLinkage_evidence::eType_unspecified);
        }
        break;
    case eLinkEvid_Forbidden:
        if( ! setOfLinkageEvidence.empty() ) {
            FASTA_WARNING(LineNumber(),
                "FASTA-Reader: This gap-type cannot have any "
                "linkage-evidence specified, so any will be ignored.",
                ILineError::eProblem_ModifierFoundButNoneExpected,
                "gapline" );
            setOfLinkageEvidence.clear();
        }
        break;
    case eLinkEvid_Required:
        if( setOfLinkageEvidence.empty() ) {
            setOfLinkageEvidence.insert(CLinkage_evidence::eType_unspecified);
        }
        if( setOfLinkageEvidence.size() == 1 &&
            *setOfLinkageEvidence.begin() == CLinkage_evidence::eType_unspecified)
        {
            FASTA_WARNING(LineNumber(),
                "CFastaReader: This gap-type should have at least one "
                "specified linkage-evidence.",
                ILineError::eProblem_ExpectedModifierMissing,
                "gapline" );
        }
        break;
        // intentionally omitted "default:" so a compiler warning will
        // hopefully let us know if we've forgotten a case
    }

    TGapRef pGap( new SGap(
        uPos, uGapSize, eIsKnown, LineNumber(), pGapType, 
        setOfLinkageEvidence ) );

    m_Gaps.push_back(pGap);
    m_TotalGapLength += pGap->m_uLen;
    return true;
}

void CFastaReader::AssembleSeq(IMessageListener * pMessageListener)
{
    CSeq_inst& inst = m_CurrentSeq->SetInst();

    CloseGap();
    CloseMask();
    if (TestFlag(fInSegSet)) {
        m_SegmentBase += GetCurrentPos(ePosWithGaps);
    }
    AssignMolType(pMessageListener);

    // apply source mods *after* figuring out mol type
    ITERATE(vector<SLineTextAndLoc>, title_ci, m_CurrentSeqTitles) {
        ParseTitle(*title_ci, pMessageListener);
    }
    m_CurrentSeqTitles.clear();

    CSeq_data::E_Choice format
        = ( inst.IsAa() ?
            ( TestFlag(fUseIupacaa) ? 
              CSeq_data::e_Iupacaa : 
              CSeq_data::e_Ncbieaa ) :
            CSeq_data::e_Iupacna);
    if (TestFlag(fValidate)) {
        CSeq_data tmp_data(m_SeqData, format);
        vector<TSeqPos> badIndexes;
        CSeqportUtil::Validate(tmp_data, &badIndexes);
        if ( ! badIndexes.empty() ) {
            NCBI_THROW2(CBadResiduesException, eBadResidues,
                "CFastaReader: Invalid " + x_NucOrProt() + "residue(s) in input sequence",
                CBadResiduesException::SBadResiduePositions( m_BestID, badIndexes, LineNumber() ) );
        }
    }

    if ( !TestFlag(fParseGaps)  &&  m_TotalGapLength > 0 ) {
        // Encountered >? lines; substitute runs of Ns or Xs as appropriate.
        string    new_data;
        char      gap_char(inst.IsAa() ? 'X' : 'N');
        SIZE_TYPE pos = 0;
        new_data.reserve(GetCurrentPos(ePosWithGaps));
        ITERATE (TGaps, it, m_Gaps) {
            // since we're not parsing gaps, we have to throw out
            // any specified extra information that can't be
            // represented with a mere 'X' or 'N', so at least
            // give a warning
            const bool bHasSpecifiedGapType = 
                ( (*it)->m_pGapType && *(*it)->m_pGapType != CSeq_gap::eType_unknown );
            const bool bHasSpecifiedLinkEvid =
                ( ! (*it)->m_setOfLinkageEvidence.empty() &&
                  ( (*it)->m_setOfLinkageEvidence.size() > 1 ||
                    *(*it)->m_setOfLinkageEvidence.begin() != CLinkage_evidence::eType_unspecified ) );
            if( bHasSpecifiedGapType || bHasSpecifiedLinkEvid )
            {
                FASTA_WARNING((*it)->m_uLineNumber,
                    "CFastaReader: Gap mods are ignored because gaps are "
                    "becoming N's or X's in this case.",
                    ILineError::eProblem_ModifierFoundButNoneExpected,
                    "gapline" );
            }
            if ((*it)->m_uPos > pos) {
                new_data.append(m_SeqData, pos, (*it)->m_uPos - pos);
                pos = (*it)->m_uPos;
            }
            new_data.append((*it)->m_uLen, gap_char);
        }
        if (m_CurrentPos > pos) {
            new_data.append(m_SeqData, pos, m_CurrentPos - pos);
        }
        swap(m_SeqData, new_data);
        m_Gaps.clear();
        m_CurrentPos += m_TotalGapLength;
        m_TotalGapLength = 0;
        m_CurrentGapChar = '\0';
    }

    if (m_Gaps.empty()) {
        _ASSERT(m_TotalGapLength == 0);
        if (m_SeqData.empty()) {
            inst.SetLength(0);
            inst.SetRepr(CSeq_inst::eRepr_virtual);
            // empty sequence triggers warning if seq data was expected
            if( ! TestFlag(fDisableNoResidues) &&
                ! TestFlag(fNoSeqData) ) {
                FASTA_ERROR(LineNumber(),
                    "FASTA-Reader: No residues given",
                    eNoResidues);
            }
        } else if (TestFlag(fNoSplit)) {
            inst.SetLength(GetCurrentPos(eRawPos));
            inst.SetRepr(CSeq_inst::eRepr_raw);
            CRef<CSeq_data> data(new CSeq_data(m_SeqData, format));
            if ( !TestFlag(fLeaveAsText) ) {
                CSeqportUtil::Pack(data, inst.GetLength());
            }
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

            if (i == 0  &&  m_Gaps[i]->m_uPos > 0) {
                TStr chunk(m_SeqData, 0, m_Gaps[i]->m_uPos);
                if (TestFlag(fNoSplit)) {
                    delta_ext.AddLiteral(chunk, inst.GetMol());
                } else {
                    delta_ext.AddAndSplit(chunk, format, m_Gaps[i]->m_uPos,
                                          TestFlag(fLetterGaps));
                }
            }

            // add delta-seq
            CRef<CDelta_seq> gap_ds(new CDelta_seq);
            if (m_Gaps[i]->m_uLen == 0) { // totally unknown
                gap_ds->SetLoc().SetNull();
            } else { // has a nominal length (normally 100)
                gap_ds->SetLiteral().SetLength(m_Gaps[i]->m_uLen);
                if( m_Gaps[i]->m_eKnownSize == SGap::eKnownSize_No ) {
                    gap_ds->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                }

                if( m_Gaps[i]->m_pGapType || ! m_Gaps[i]->m_setOfLinkageEvidence.empty() ) {
                    CSeq_gap & seq_gap = gap_ds->SetLiteral().SetSeq_data().SetGap();
                    seq_gap.SetType( m_Gaps[i]->m_pGapType ? 
                        *m_Gaps[i]->m_pGapType : 
                        CSeq_gap::eType_unknown );

                    // set linkage and linkage-evidence, if any
                    if( ! m_Gaps[i]->m_setOfLinkageEvidence.empty() ) {
                        // any linkage-evidence (even "unspecified")
                        // implies "linked"
                        seq_gap.SetLinkage( CSeq_gap::eLinkage_linked );

                        CSeq_gap::TLinkage_evidence & vecLinkEvids = 
                            seq_gap.SetLinkage_evidence();
                        ITERATE(SGap::TLinkEvidSet, link_evid_it, 
                            m_Gaps[i]->m_setOfLinkageEvidence ) 
                        {
                            CSeq_gap::TLinkage_evidence::value_type pNewLinkEvid(
                                new CLinkage_evidence );
                            pNewLinkEvid->SetType( *link_evid_it );
                            vecLinkEvids.push_back(pNewLinkEvid);
                        }
                    }
                }
            }
            delta_ext.Set().push_back(gap_ds);

            TSeqPos next_start = (i == n-1) ? m_CurrentPos : m_Gaps[i+1]->m_uPos;
            if (next_start != m_Gaps[i]->m_uPos) {
                TSeqPos seq_len = next_start - m_Gaps[i]->m_uPos;
                TStr chunk(m_SeqData, m_Gaps[i]->m_uPos, seq_len);
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

void CFastaReader::AssignMolType(IMessageListener * pMessageListener)
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
                        "CFastaReader: Unable to determine sequence type (is it nucleotide? protein?) around line " + NStr::NumericToString(LineNumber()),
                        LineNumber());
        } else {
            inst.SetMol(default_mol);
        }
    } 
}

bool
CFastaReader::CreateWarningsForSeqDataInTitle(
    const TStr& sLineText, 
    TSeqPos iLineNum,
    IMessageListener * pMessageListener)
{
    bool bFoundProblem = false;

    // check for nuc or aa sequences at the end of the title
    const static size_t kWarnNumNucCharsAtEnd = 20;
    const static size_t kWarnAminoAcidCharsAtEnd = 50;
    if( sLineText.length() > kWarnNumNucCharsAtEnd ) {

        // find last non-nuc character, within the last kWarnNumNucCharsAtEnd characters
        SIZE_TYPE pos_to_check = (sLineText.length() - 1);
        const SIZE_TYPE last_pos_to_check_for_nuc = (sLineText.length() - kWarnNumNucCharsAtEnd);
        for( ; pos_to_check >= last_pos_to_check_for_nuc; --pos_to_check ) {
            if( ! s_ASCII_IsUnAmbigNuc(sLineText[pos_to_check]) ) {
                // found a character which is not an unambiguous nucleotide
                break;
            }
        }
        if( pos_to_check < last_pos_to_check_for_nuc ) {
            FASTA_WARNING(iLineNum,
                "FASTA-Reader: Title ends with at least " << kWarnNumNucCharsAtEnd 
                << " valid nucleotide characters.  Was the sequence "
                << "accidentally put in the title line?",
                ILineError::eProblem_UnexpectedNucResidues,
                "defline"
                );
            bFoundProblem = true;
        } else if( sLineText.length() > kWarnAminoAcidCharsAtEnd ) {
            // check for aa's at the end of the title
            // for efficiency, continue where the nuc search left off, since
            // we know that nucs can be amino acids, also
            const SIZE_TYPE last_pos_to_check_for_amino_acid =
                ( sLineText.length() - kWarnAminoAcidCharsAtEnd );
            for( ; pos_to_check >= last_pos_to_check_for_amino_acid; --pos_to_check ) {
                // can't just use "isalpha" in case it includes characters
                // with diacritics (an accent, tilde, umlaut, etc.)
                const char ch = sLineText[pos_to_check];
                if( ( ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ) {
                    // potential amino acid, so keep going
                } else {
                    // non-amino-acid found
                    break;
                }
            }

            if( pos_to_check < last_pos_to_check_for_amino_acid ) {
                FASTA_WARNING(iLineNum,
                    "FASTA-Reader: Title ends with at least " << kWarnAminoAcidCharsAtEnd
                    << " valid amino acid characters.  Was the sequence "
                    << "accidentally put in the title line?",
                    ILineError::eProblem_UnexpectedAminoAcids,
                    "defline");
                bFoundProblem = true;
            }
        }
    }

    return bFoundProblem;
}

CRef<CSeq_entry> CFastaReader::ReadAlignedSet(
    int reference_row, IMessageListener * pMessageListener)
{
    TIds             ids;
    CRef<CSeq_entry> entry = x_ReadSeqsToAlign(ids, pMessageListener);
    CRef<CSeq_annot> annot(new CSeq_annot);

    if ( !entry->IsSet()
        ||  entry->GetSet().GetSeq_set().size() < 
            static_cast<unsigned int>(max(reference_row + 1, 2))) 
    {
        NCBI_THROW2(CObjReaderParseException, eEOF,
                    "CFastaReader::ReadAlignedSet: not enough input sequences.",
                    LineNumber());
    } else if (reference_row >= 0) {
        x_AddPairwiseAlignments(*annot, ids, reference_row);
    } else {
        x_AddMultiwayAlignment(*annot, ids);
    }
    entry->SetSet().SetAnnot().push_back(annot);

    entry->Parentize();
    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSeqsToAlign(TIds& ids,
    IMessageListener * pMessageListener)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    vector<TSeqPos>  lengths;

    CFlagGuard guard(m_Flags, GetFlags() | fAligning | fParseGaps);

    for (m_Row = 0, m_Starts.clear();  !GetLineReader().AtEOF();  ++m_Row) {
        try {
            // must mark m_Starts prior to reading in case of leading gaps
            m_Starts[0][m_Row] = 0;
            CRef<CSeq_entry> entry2(ReadOneSeq(pMessageListener));
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
                FASTA_ERROR(LineNumber(),
                            "CFastaReader::ReadAlignedSet: Rows have different "
                            "lengths. For example, look around line " << LineNumber(), 
                            eFormat );
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
                           int* counter, vector<CConstRef<CSeq_loc> >* lcv,
                           IMessageListener * pMessageListener)
{
    CRef<ILineReader> lr(ILineReader::New(in));
    CFastaReader      reader(*lr, flags);
    CCounterManager   counter_manager(reader.SetIDGenerator(), counter);
    if (lcv) {
        reader.SaveMasks(reinterpret_cast<CFastaReader::TMasks*>(lcv));
    }
    return reader.ReadSet(kMax_Int, pMessageListener);
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
    void ParseDefLine(const TStr& s, 
        IMessageListener * pMessageListener);
    void ParseTitle(const SLineTextAndLoc & lineInfo, 
        IMessageListener * pMessageListener = 0);
    void AssembleSeq(IMessageListener * pMessageListener);

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

void CFastaMapper::ParseDefLine(const TStr& s, IMessageListener * pMessageListener)
{
    TParent::ParseDefLine(s, pMessageListener); // We still want the default behavior.
    m_MapEntry.seq_id = GetIDs().front()->AsFastaString(); // XXX -- GetBestID?
    m_MapEntry.all_seq_ids.resize(0);
    ITERATE (CBioseq::TId, it, GetIDs()) {
        m_MapEntry.all_seq_ids.push_back((*it)->AsFastaString());
    }
    m_MapEntry.stream_offset = StreamPosition() - s.length();
}

void CFastaMapper::ParseTitle(const SLineTextAndLoc & s, 
    IMessageListener * pMessageListener)
{
    TParent::ParseTitle(s, pMessageListener);
    m_MapEntry.description = s.m_sLineText;
}

void CFastaMapper::AssembleSeq(IMessageListener * pMessageListener)
{
    TParent::AssembleSeq(pMessageListener);
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

void CFastaReader::x_ApplyAllMods( 
    CBioseq & bioseq,
    TSeqPos iLineNum,
    IMessageListener * pMessageListener )
{
    // this is called even if there the user did not request
    // mods to be added because we want to give a warning if there
    // are mods when not expected.

    CSourceModParser smp( TestFlag(fBadModThrow) ?
        CSourceModParser::eHandleBadMod_Throw : 
    CSourceModParser::eHandleBadMod_Ignore );
    smp.SetModFilter( m_pModFilter );
    CRef<CSeqdesc> title_desc;


    if( ! bioseq.IsSetDescr() && ! bioseq.GetDescr().IsSet() ) {
        return;
    }

    // find title
    // (and remember iter in case we need to delete later)
    CSeq_descr::Tdata & desc_container = bioseq.SetDescr().Set();
    CSeq_descr::Tdata::iterator desc_it = desc_container.begin();
    const CSeq_descr::Tdata::iterator desc_end = desc_container.end();
    for( ; desc_it != desc_end; ++desc_it ) {
        if( (*desc_it)->IsTitle() ) {
            title_desc.Reset( &(**desc_it) );
            break;
        }
    }

    if ( ! title_desc ) {
        return;
    }

    string& title = title_desc->SetTitle();

    if( TestFlag(fAddMods) ) {
        title = smp.ParseTitle(title, CConstRef<CSeq_id>(bioseq.GetFirstId()) );

        smp.ApplyAllMods(bioseq);
        if( TestFlag(fUnknModThrow) ) {
            CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
            if( ! unused_mods.empty() ) 
            {
                // there are unused mods and user specified to throw if any
                // unused 
                CNcbiOstrstream err;
                err << "CFastaReader: Inapplicable or unrecognized modifiers on ";

                // get sequence ID
                const CSeq_id* seq_id = bioseq.GetFirstId();
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
                err << " around line " + NStr::NumericToString(iLineNum);
                NCBI_THROW2(CObjReaderParseException, eUnusedMods,
                    (string)CNcbiOstrstreamToString(err),
                    iLineNum);
            }
        }

        smp.GetLabel(&title, CSourceModParser::fUnusedMods);

        copy( smp.GetBadMods().begin(), smp.GetBadMods().end(),
            inserter(m_BadMods, m_BadMods.begin()) );
        CSourceModParser::TMods unused_mods = 
            smp.GetMods(CSourceModParser::fUnusedMods);
        copy( unused_mods.begin(), unused_mods.end(),
            inserter(m_UnusedMods, m_UnusedMods.begin() ) );
    } else {
        // user did not request fAddMods, so we warn that we found
        // mods anyway
        smp.ParseTitle(
            title, 
            CConstRef<CSeq_id>(bioseq.GetFirstId()),
            1 // "1" since we only care whether or not there are mods, not how many
            );
        CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
        if( ! unused_mods.empty() ) {
            FASTA_WARNING(iLineNum,
                "FASTA-Reader: Ignoring FASTA modifier(s) found because "
                "the input was not expected to have any.",
                ILineError::eProblem_ModifierFoundButNoneExpected,
                "defline");
        }
    }

    // remove title if empty
    if( title.empty() ) {
        desc_container.erase(desc_it);
    }
}

std::string CFastaReader::x_NucOrProt(void) const
{
    if( m_CurrentSeq && m_CurrentSeq->IsSetInst() && 
        m_CurrentSeq->GetInst().IsSetMol() )
    {
        return ( m_CurrentSeq->GetInst().IsAa() ? "protein " : "nucleotide " );
    } else {
        return kEmptyStr;
    }
}

// static
string CFastaReader::CanonicalizeString(const TStr & sValue)
{
    string newString;
    newString.reserve(sValue.length());

    ITERATE_0_IDX(ii, sValue.length()) {
        const char ch = sValue[ii];
        if( isupper(ch) ) {
            newString.push_back(tolower(ch));
        } else if( ch == ' ' || ch == '_' ) {
            newString.push_back('-');
        } else {
            newString.push_back(ch);
        }
    }

    return newString;
}

// static
const CFastaReader::SGapTypeInfo *
CFastaReader::NameToGapTypeInfo(const CTempString & sName)
{
    const CFastaReader::TGapTypeMap & gapTypeMap =
        GetNameToGapTypeInfoMap();

    TGapTypeMap::const_iterator find_iter =
        gapTypeMap.find( CanonicalizeString(sName).c_str() );
    if( find_iter == gapTypeMap.end() ) {
        // not found
        return NULL;
    } else {
        return & find_iter->second;
    }
}

// static
const CFastaReader::TGapTypeMap & 
CFastaReader::GetNameToGapTypeInfoMap(void)
{
    // gap-type name to info
    typedef SStaticPair<const char*, SGapTypeInfo> TGapTypeElem;
    static const TGapTypeElem sc_gap_type_map[] = {
        {"between-scaffolds",        { CSeq_gap::eType_contig,          eLinkEvid_Required} },
        {"centromere",               { CSeq_gap::eType_centromere,      eLinkEvid_Forbidden} },
        {"heterochromatin",          { CSeq_gap::eType_heterochromatin, eLinkEvid_Forbidden} },
        {"repeat-between-scaffolds", { CSeq_gap::eType_repeat,          eLinkEvid_Forbidden} },
        {"repeat-within-scaffold",   { CSeq_gap::eType_repeat,          eLinkEvid_Required} },
        {"short-arm",                { CSeq_gap::eType_short_arm,       eLinkEvid_Forbidden} },
        {"telomere",                 { CSeq_gap::eType_telomere,        eLinkEvid_Forbidden} },
        {"unknown",                  { CSeq_gap::eType_unknown,         eLinkEvid_UnspecifiedOnly} },
        {"within-scaffold",          { CSeq_gap::eType_scaffold,        eLinkEvid_Forbidden} },
    };
    DEFINE_STATIC_ARRAY_MAP(TGapTypeMap, sc_GapTypeMap, sc_gap_type_map);
    return sc_GapTypeMap;
}

CFastaReader::SGap::SGap(
    TSeqPos uPos,
    TSignedSeqPos uLen,
    EKnownSize eKnownSize,
    TSeqPos uLineNumber,
    TNullableGapType pGapType,
    const set<CLinkage_evidence::EType> & setOfLinkageEvidence ) :
        m_uPos(uPos),
        m_uLen(uLen),
        m_eKnownSize(eKnownSize),
        m_uLineNumber(uLineNumber),
        m_pGapType(pGapType),
        m_setOfLinkageEvidence(setOfLinkageEvidence)
{
}

END_SCOPE(objects)
END_NCBI_SCOPE

