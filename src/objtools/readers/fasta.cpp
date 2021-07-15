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
#include <objtools/readers/fasta_reader_utils.hpp>
#include <objtools/readers/mod_reader.hpp>

#include <ctype.h>

// The "49518053" is just a random number to minimize the chance of the
// variable name conflicting with another variable name and has no
// particular meaning
#define FASTA_LINE_EXPT(_eSeverity, _uLineNum, _MessageStrmOps,  _eErrCode, _eProblem, _sFeature, _sQualName, _sQualValue) \
    do {                                                                \
        stringstream err_strm_49518053;                                 \
        err_strm_49518053 << _MessageStrmOps;                           \
        PostWarning(pMessageListener, (_eSeverity), (_uLineNum), (err_strm_49518053.str()),  (_eErrCode), (_eProblem), (_sFeature), (_sQualName), (_sQualValue)); \
    } while(0)

// The "49518053" is just a random number to minimize the chance of the
// variable name conflicting with another variable name and has no
// particular meaning
#define FASTA_PROGRESS(_MessageStrmOps)                                 \
    do {                                                                \
        stringstream err_strm_49518053;                                 \
        err_strm_49518053 << _MessageStrmOps;                           \
        if( pMessageListener ) {                                        \
            pMessageListener->PutProgress(err_strm_49518053.str());     \
        }                                                               \
    } while(false)


#define FASTA_WARNING(_uLineNum, _MessageStrmOps, _eProblem, _Feature) \
    FASTA_LINE_EXPT(eDiag_Warning, _uLineNum, _MessageStrmOps, CObjReaderParseException::eFormat, _eProblem, _Feature, kEmptyStr, kEmptyStr)

#define FASTA_WARNING_EX(_uLineNum, _MessageStrmOps, _eProblem, _Feature, _sQualName, _sQualValue) \
    FASTA_LINE_EXPT(eDiag_Warning, _uLineNum, _MessageStrmOps, CObjReaderParseException::eFormat, _eProblem, _Feature, _sQualName, _sQualValue)

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

// The arg *must* be a lowercase letter or this won't work
inline unsigned char s_ASCII_MustBeLowerToUpper(unsigned char c)
{
    return c + ('A' - 'a');
}

inline bool s_ASCII_IsAmbigNuc(unsigned char c)
{
    switch(c) {
    case 'U': case 'u':
    case 'R': case 'r':
    case 'Y': case 'y':
    case 'S': case 's':
    case 'W': case 'w':
    case 'K': case 'k':
    case 'M': case 'm':
    case 'B': case 'b':
    case 'D': case 'd':
    case 'H': case 'h':
    case 'V': case 'v':
    case 'N': case 'n':
        return true;
    default:
        return false;
    }
}

inline static bool s_ASCII_IsUnAmbigNuc(unsigned char c)
{
    switch( c ) {
    case 'A': case 'C': case 'G': case 'T':
    case 'a': case 'c': case 'g': case 't':
        return true;
    default:
        return false;
    }
}

void
CFastaReader::AddStringFlags(
    const list<string>& stringFlags,
    TFlags& baseFlags)
{
    static const map<string, CFastaReader::TReaderFlags> flagsMap = {
        { "AssumeNuc",    CFastaReader::fAssumeNuc},
        { "AssumeProt",   CFastaReader::fAssumeProt},
        { "ForceType",    CFastaReader::fForceType},
        { "NoParseID",    CFastaReader::fNoParseID},
        { "ParseGaps",    CFastaReader::fParseGaps},
        { "OneSeq",       CFastaReader::fOneSeq},
        { "NoSeqData",    CFastaReader::fNoSeqData},
        { "RequireID",    CFastaReader::fRequireID},
        { "DLOptional",   CFastaReader::fDLOptional},
        { "ParseRawID",   CFastaReader::fParseRawID},
        { "SkipCheck",    CFastaReader::fSkipCheck},
        { "NoSplit",      CFastaReader::fNoSplit},
        { "Validate",     CFastaReader::fValidate},
        { "UniqueIDs",    CFastaReader::fUniqueIDs},
        { "StrictGuess",  CFastaReader::fStrictGuess},
        { "LaxGuess",     CFastaReader::fLaxGuess},
        { "AddMods",      CFastaReader::fAddMods},
        { "LetterGaps",   CFastaReader::fLetterGaps},
        { "NoUserObjs",   CFastaReader::fNoUserObjs},
        { "LeaveAsText",  CFastaReader::fLeaveAsText},
        { "QuickIDCheck", CFastaReader::fQuickIDCheck},
        { "UseIupacaa",   CFastaReader::fUseIupacaa},
        { "HyphensIgnoreAndWarn", CFastaReader::fHyphensIgnoreAndWarn},
        { "DisableNoResidues",    CFastaReader::fDisableNoResidues},
        { "DisableParseRange",    CFastaReader::fDisableParseRange},
        { "IgnoreMods",           CFastaReader::fIgnoreMods}
    };

    return CReaderBase::xAddStringFlagsWithMap(stringFlags, flagsMap, baseFlags);
};


CFastaReader::CFastaReader(ILineReader& reader, TFlags flags, FIdCheck f_idcheck)
    : m_LineReader(&reader), m_MaskVec(0),
      m_gapNmin(0), m_gap_Unknown_length(0),
      m_MaxIDLength(kMax_UI4),
      m_fIdCheck(f_idcheck)
{
    m_Flags.push(flags);
    m_IDHandler = Ref(new CFastaIdHandler());
}

CFastaReader::CFastaReader(CNcbiIstream& in, TFlags flags, FIdCheck f_idcheck)
    : CFastaReader(*(ILineReader::New(in)), flags, f_idcheck) {}

CFastaReader::CFastaReader(const string& path, TFlags flags, FIdCheck f_idcheck)
    : CFastaReader(*(ILineReader::New(path)), flags, f_idcheck) {}

CFastaReader::CFastaReader(CReaderBase::TReaderFlags fBaseFlags, TFlags flags, FIdCheck f_idcheck)
    : CReaderBase(fBaseFlags), m_MaskVec(0),
      m_gapNmin(0), m_gap_Unknown_length(0),
      m_MaxIDLength(kMax_UI4),
      m_fIdCheck(f_idcheck)
{
    m_Flags.push(flags);
    m_IDHandler = Ref(new CFastaIdHandler());
}

CFastaReader::~CFastaReader(void)
{
    _ASSERT(m_Flags.size() == 1);
}

void CFastaReader::SetMinGaps(TSeqPos gapNmin, TSeqPos gap_Unknown_length)
{
    m_gapNmin = gapNmin; m_gap_Unknown_length = gap_Unknown_length;
}

CRef<CSerialObject>
CFastaReader::ReadObject (ILineReader &lr, ILineErrorListener *pMessageListener)
{
    CRef<CSerialObject> object(
        ReadSeqEntry( lr, pMessageListener ).ReleaseOrNull() );
    return object;
}

CRef<CSeq_entry>
CFastaReader::ReadSeqEntry (ILineReader &lr, ILineErrorListener *pMessageListener)
{
    CRef<ILineReader> pTempLineReader( &lr );
    CTempRefSwap<ILineReader> tempRefSwap(m_LineReader, pTempLineReader);

    return ReadSet(kMax_Int, pMessageListener);
}

CRef<CSeq_entry> CFastaReader::ReadOneSeq(ILineErrorListener * pMessageListener)
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
                        CObjReaderParseException::eEOF );
            break;
        }
        if (c == '>' ) {
            CTempString next_line = *++GetLineReader();
            string strmodified;
            if( NStr::StartsWith(next_line, ">?_") ) {
                CTempString tmp = next_line.substr(3);
                strmodified = ">";
                strmodified.append(tmp.data(), tmp.length());
                next_line = strmodified;
            }
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
        }

        CTempString line = NStr::TruncateSpaces_Unsafe(*++GetLineReader());

        if (line.empty()) {
            continue; // ignore lines containing only whitespace
        }
        c = line[0];

        if (c == '!'  ||  c == '#' || c == ';') {
            // no content, just a comment or blank line
            continue;
        } else if (need_defline) {
            if (TestFlag(fDLOptional)) {
                ParseDefLine(">", pMessageListener);
                need_defline = false;
            } else {
                const auto lineNum = LineNumber();
                GetLineReader().UngetLine();
                NCBI_THROW2(CObjReaderParseException, eNoDefline,
                            "CFastaReader: Input doesn't start with"
                            " a defline or comment around line " + NStr::NumericToString(lineNum),
                             lineNum);
            }
        }

        if ( !TestFlag(fNoSeqData) ) {
            try {
                string strmodified;
                if( NStr::StartsWith(line, ">?_") ) {
                    CTempString tmp = line.substr(3);
                    strmodified = ">";
                    strmodified.append(tmp.data(), tmp.length());
                    line = strmodified;
                }
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
            CObjReaderParseException::eEOF);
    }

    AssembleSeq(pMessageListener);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*m_CurrentSeq);

    entry->Parentize();
    return entry;
}

CRef<CSeq_entry> CFastaReader::ReadSet(int max_seqs, ILineErrorListener * pMessageListener)
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


    if (entry->IsSet()  &&  entry->GetSet().GetSeq_set().size() == 1) {
        return entry->SetSet().SetSeq_set().front();
    } else {
        entry->Parentize();
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
    m_IDHandler->SetGenerator(gen);
}

void CFastaReader::SetMaxIDLength(Uint4 max_len)
{
    CFastaDeflineReader::s_MaxLocalIDLength =
    CFastaDeflineReader::s_MaxGeneralTagLength =
    CFastaDeflineReader::s_MaxAccessionLength = m_MaxIDLength = max_len;
    m_bModifiedMaxIdLength=true;
}


// For reasons of efficiency, this method does not use
// CRef<CSeq_interval> to access range information - RW-26
void CFastaReader::ParseDefLine(const TStr& defLine,
                                const SDefLineParseInfo& info,
                                const TIgnoredProblems& ignoredErrors,
                                list<CRef<CSeq_id>>& ids,
                                bool& hasRange,
                                TSeqPos& rangeStart,
                                TSeqPos& rangeEnd,
                                TSeqTitles& seqTitles,
                                ILineErrorListener* pMessageListener)
{
    CFastaDeflineReader::SDeflineData data;
    CFastaDeflineReader::ParseDefline(
        defLine,
        info,
        data,
        pMessageListener);

    ids = move(data.ids);
    hasRange   = data.has_range;
    rangeStart = data.range_start;
    rangeEnd   = data.range_end;
    seqTitles  = move(data.titles);
}


CRef<CSeq_align> CFastaReader::xCreateAlignment(CRef<CSeq_id> old_id,
                CRef<CSeq_id> new_id,
                TSeqPos range_start,
                TSeqPos range_end)
{
    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(CSeq_align::eType_partial); // ?
    align->SetDim(2);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.SetDim(2); // redundant, but required by validator
    denseg.SetIds().push_back(new_id);
    denseg.SetIds().push_back(old_id);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(range_start);
    if (range_start > range_end) { // negative strand
        denseg.SetLens().push_back(range_start + 1 - range_end);
        denseg.SetStrands().push_back(eNa_strand_plus);
        denseg.SetStrands().push_back(eNa_strand_minus);
    } else {
        denseg.SetLens().push_back(range_end + 1 - range_start);
    }

    return align;
}


bool CFastaReader::xSetSeqMol(const list<CRef<CSeq_id>>& ids, CSeq_inst_Base::EMol& mol) {

    for (auto pId : ids) {
        const CSeq_id::EAccessionInfo acc_info = pId->IdentifyAccession();
        if (acc_info & CSeq_id::fAcc_nuc) {
            mol = CSeq_inst::eMol_na;
            return true;
        }

        if (acc_info & CSeq_id::fAcc_prot) {
            mol = CSeq_inst::eMol_aa;
            return true;
        }
    }
    return false;
}


void CFastaReader::ParseDefLine(const TStr& s, ILineErrorListener * pMessageListener)
{
    SDefLineParseInfo parseInfo;
    x_SetDeflineParseInfo(parseInfo);

    CFastaDeflineReader::SDeflineData data;
    CFastaDeflineReader::ParseDefline(s, parseInfo, data, pMessageListener, m_fIdCheck);

    m_CurrentSeqTitles = move(data.titles);

    if (data.ids.empty()) {
        if (TestFlag(fRequireID)) {
            // No [usable] IDs
            FASTA_ERROR(LineNumber(),
                "CFastaReader: Defline lacks a proper ID around line " << LineNumber(),
                CObjReaderParseException::eNoIDs );
        }
    }
    else if (!TestFlag(fForceType)) {
        CSeq_inst::EMol mol;
        if (xSetSeqMol(data.ids, mol)) {
            m_CurrentSeq->SetInst().SetMol(mol);
        }
    }

    PostProcessIDs(data.ids, s, data.has_range, data.range_start, data.range_end);

    m_BestID = FindBestChoice(GetIDs(), CSeq_id::BestRank);
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
            if ( !m_IDHandler->CacheIdHandle(h) ) {
                FASTA_ERROR(LineNumber(),
                    "CFastaReader: Seq-id " << h.AsString()
                    << " is a duplicate around line " << LineNumber(),
                    CObjReaderParseException::eDuplicateID );
            }
        }
    }
}


void CFastaReader::PostProcessIDs(
    const CBioseq::TId& defline_ids,
    const string& defline,
    const bool has_range,
    const TSeqPos range_start,
    const TSeqPos range_end)
{
    if (defline_ids.empty()) {
        GenerateID();
    }
    else {
        SetIDs() = defline_ids;
    }

    if (has_range) {
        auto old_id = FindBestChoice(GetIDs(), CSeq_id::BestRank);
        // generate a new ID, and record its relation to the given one(s).
        SetIDs().clear();
        GenerateID();

        CRef<CSeq_align> align = xCreateAlignment(old_id,
            GetIDs().front(), range_start, range_end);

        m_CurrentSeq->SetInst().SetHist().SetAssembly().push_back(align);
    }
}


void CFastaReader::x_SetDeflineParseInfo(SDefLineParseInfo& info)
{
    info.fBaseFlags = m_iFlags;
    info.fFastaFlags = GetFlags();
    info.maxIdLength = m_bModifiedMaxIdLength ?
                       m_MaxIDLength :
                       0;
    info.lineNumber = LineNumber();
}


size_t CFastaReader::ParseRange(
    const TStr& s, TSeqPos& start, TSeqPos& end,
    ILineErrorListener* pMessageListener)
{
    return CFastaDeflineReader::ParseRange(
        s,
        start,
        end,
        pMessageListener);
}


void CFastaReader::ParseTitle(
    const SLineTextAndLoc & lineInfo, ILineErrorListener * pMessageListener)
{
    const static size_t kWarnTitleLength = 1000;
    if( lineInfo.m_sLineText.length() > kWarnTitleLength ) {
        FASTA_WARNING(lineInfo.m_iLineNum,
            "FASTA-Reader: Title is very long: " << lineInfo.m_sLineText.length()
            << " characters (max is " << kWarnTitleLength << ")",
            ILineError::eProblem_TooLong, "defline");
    }

    CreateWarningsForSeqDataInTitle(lineInfo.m_sLineText,lineInfo.m_iLineNum, pMessageListener);

    CTempString title(lineInfo.m_sLineText.data(), lineInfo.m_sLineText.length());
    x_ApplyMods(title, lineInfo.m_iLineNum, *m_CurrentSeq, pMessageListener);
}


bool CFastaReader::IsValidLocalID(const TStr& s) const
{
    return IsValidLocalID(s, GetFlags());
}

bool CFastaReader::IsValidLocalID(const TStr& idString,
    const TFlags fFastaFlags)
{
    if ( fFastaFlags & fQuickIDCheck) { // check only the first character
        return CSeq_id::IsValidLocalID(idString.substr(0,1));
    }

    return CSeq_id::IsValidLocalID(idString);
}

void CFastaReader::GenerateID(void)
{
    CRef<CSeq_id> id = m_IDHandler->GenerateID(TestFlag(fUniqueIDs));
    SetIDs().push_back(id);
}


void CFastaReader::CheckDataLine(
    const TStr& s, ILineErrorListener * pMessageListener)
{
    // make sure the first data line has at least SOME resemblance to
    // actual sequence data.
    if (TestFlag(fSkipCheck)  ||  ! m_SeqData.empty() ) {
        return;
    }
    const bool bIgnoreHyphens = TestFlag(fHyphensIgnoreAndWarn);
    size_t good = 0, bad = 0;
    // in case the data has huge sequences all on the first line we do need
    // a cutoff and "70" seems reasonable since it's the default width of
    // CFastaOstream (as of 2017-03-09)
    size_t len_to_check = min(s.length(),
                              static_cast<size_t>(70));
    const bool bIsNuc = (
        ( TestFlag(fAssumeNuc) && TestFlag(fForceType) ) ||
        ( m_CurrentSeq && m_CurrentSeq->IsSetInst() &&
          m_CurrentSeq->GetInst().IsSetMol() &&  m_CurrentSeq->IsNa() ) );
    size_t ambig_nuc = 0;
    for (size_t pos = 0;  pos < len_to_check;  ++pos) {
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
    if (bad >= good / 3  &&
        (len_to_check > 3  ||  good == 0  ||  bad > good))
    {
        FASTA_ERROR( LineNumber(),
            "CFastaReader: Near line " << LineNumber()
            << ", there's a line that doesn't look like plausible data, "
            "but it's not marked as defline or comment.",
            CObjReaderParseException::eFormat);
    }
    // warn if more than a certain percentage is ambiguous nucleotides
    const static size_t kWarnPercentAmbiguous = 40; // e.g. "40" means "40%"
    const size_t percent_ambig = (good == 0)?100:((ambig_nuc * 100) / good);
    if( len_to_check > 3 && percent_ambig > kWarnPercentAmbiguous ) {
        FASTA_WARNING(LineNumber(),
            "FASTA-Reader: Start of first data line in seq is about "
            << percent_ambig << "% ambiguous nucleotides (shouldn't be over "
            << kWarnPercentAmbiguous << "%)",
            ILineError::eProblem_TooManyAmbiguousResidues,
            "first data line");
    }
}


void CFastaReader::ParseDataLine(
    const TStr& s, ILineErrorListener * pMessageListener)
{
    if( NStr::StartsWith(s, ">?") ) {
        ParseGapLine(s, pMessageListener);
        return;
    }

    CheckDataLine(s, pMessageListener);

    // most lines won't have a comment (';') so optimize for that case as
    // much as possible

    const size_t s_len = s.length(); // avoid checking over and over

    if (m_SeqData.capacity() < m_SeqData.size() + s_len) {
        // ensure exponential capacity growth to avoid quadratic runtime
        m_SeqData.reserve(2 * max(m_SeqData.capacity(), s_len));
    }

    if ((GetFlags() & (fSkipCheck | fParseGaps | fValidate)) == fSkipCheck
        &&  m_CurrentMask.Empty())
    {
        // copy until comment char or end of line
        size_t pos = 0;
        char c = '\0';
        for( ; pos < s_len && (c = s[pos]) != ';'; ++pos) {
            m_SeqData.push_back(c);
        }
        m_CurrentPos += pos;
        return;
    }

    // we're stricter with nucs, so try to determine if we should
    // assume this is a nuc
    const bool bIsNuc = (
        (! TestFlag(fForceType) &&
            FIELD_CHAIN_OF_2_IS_SET(*m_CurrentSeq, Inst, Mol))
        ? m_CurrentSeq->IsNa()
        : TestFlag(fAssumeNuc)
    );

    m_SeqData.resize(m_CurrentPos + s_len);

    // these will stay as -1 and empty unless there's an error
    int bad_pos_line_num = -1;
    vector<TSeqPos> bad_pos_vec;

    const bool bHyphensIgnoreAndWarn = TestFlag(fHyphensIgnoreAndWarn);
    const bool bHyphensAreGaps =
        ( TestFlag(fParseGaps) && ! bHyphensIgnoreAndWarn );
    const bool bAllowLetterGaps =
        ( TestFlag(fParseGaps) && TestFlag(fLetterGaps) );

    bool bIgnorableHyphenSeen = false;

    // indicates how the char should be treated
    enum ECharType {
        eCharType_NormalNonGap,
        eCharType_MaskedNonGap,
        eCharType_Gap,
        eCharType_JustIgnore,
        eCharType_HyphenToIgnoreAndWarn,
        eCharType_Comment,
        eCharType_Bad,
    };

    for (size_t pos = 0;  pos < s_len;  ++pos) {
        const unsigned char c = s[pos];

        // figure out what exactly should be done with the char
        ECharType char_type = eCharType_Bad;
        switch(c) {
        // try to keep cases with consecutive letters on the same line
        // and try to have all cases with the same result in alphabetical
        // order.  This will to make it easier to
        // tell if a letter was skipped or entered mult times

        // some cases just set char_type but others can be implemented right
        // in the first switch followed by a continue.  Try to minimize the
        // latter because they can make this switch very long

        case 'A': case 'B': case 'C': case 'D':
        case 'G': case 'H':
        case 'K':
        case 'M':
        case 'R': case 'S': case 'T': case 'U': case 'V': case 'W':
        case 'Y':
            CloseGap(pos == 0);
            m_SeqData[m_CurrentPos] = c;
            CloseMask();
            ++m_CurrentPos;
            continue;
        case 'a': case 'b': case 'c': case 'd':
        case 'g': case 'h':
        case 'k':
        case 'm':
        case 'r': case 's': case 't': case 'u': case 'v': case 'w':
        case 'y':
            char_type = eCharType_MaskedNonGap;
            break;

        case 'E': case 'F':
        case 'I': case 'J':
        case 'L':
        case 'O': case 'P': case 'Q':
        case 'Z':
        case '*':
            if( bIsNuc ) {
                char_type = eCharType_Bad;
            } else {
                CloseGap(pos == 0);
                m_SeqData[m_CurrentPos] = c;
                CloseMask();
                ++m_CurrentPos;
                continue;
            }
            break;
        case 'e': case 'f':
        case 'i': case 'j':
        case 'l':
        case 'o': case 'p': case 'q':
        case 'z':
            char_type = (bIsNuc ? eCharType_Bad : eCharType_MaskedNonGap );
            break;

        case 'N':
            char_type = ( bIsNuc && bAllowLetterGaps ?
                     eCharType_Gap : eCharType_NormalNonGap );
            break;
        case 'n':
            char_type = ( bIsNuc && bAllowLetterGaps ?
                     eCharType_Gap : eCharType_MaskedNonGap );
            break;

        case 'X':
            char_type = ( bIsNuc ? eCharType_Bad :
                     bAllowLetterGaps ? eCharType_Gap :
                     eCharType_NormalNonGap);
            break;
        case 'x':
            char_type = ( bIsNuc ? eCharType_Bad :
                     bAllowLetterGaps ? eCharType_Gap :
                     eCharType_MaskedNonGap);
            break;

        case '-':
            char_type = (
                bHyphensAreGaps ? eCharType_Gap :
                bHyphensIgnoreAndWarn ? eCharType_HyphenToIgnoreAndWarn :
                eCharType_NormalNonGap );
            break;
        case ';':
            char_type = eCharType_Comment;
            break;

        case '\t': case '\n': case '\v': case '\f': case '\r': case ' ':
            continue;

        default:
            char_type = eCharType_Bad;
            break;
        }

        switch(char_type) {
        case eCharType_NormalNonGap:
            CloseGap(pos == 0);
            m_SeqData[m_CurrentPos] = c;
            CloseMask();
            ++m_CurrentPos;
            break;
        case eCharType_MaskedNonGap:
            CloseGap(pos == 0);
            m_SeqData[m_CurrentPos] = s_ASCII_MustBeLowerToUpper(c);
            OpenMask();
            ++m_CurrentPos;
            break;
        case eCharType_Gap: {
            CloseMask();
            // open a gap

            size_t pos2 = pos + 1;
            while( pos2 < s_len && s[pos2] == c ) {
                ++pos2;
            }
            _ASSERT(pos2 <= s_len);
            m_CurrentGapLength += pos2 - pos;
            m_CurrentGapChar = toupper(c);
            pos = pos2 - 1; // `- 1` compensates for the `++pos` in the `for`
            break;
        }
        case eCharType_JustIgnore:
            break;
        case eCharType_HyphenToIgnoreAndWarn:
            bIgnorableHyphenSeen = true;
            break;
        case eCharType_Comment:
            // artificially advance pos to the end to break the pos loop
            pos = s_len;
            break;
        case eCharType_Bad:
            if( bad_pos_line_num < 0 ) {
                bad_pos_line_num = LineNumber();
            }
            bad_pos_vec.push_back(pos);
            break;
        default:
            _TROUBLE;
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
    TSeqPos len, bool atStartOfLine, ILineErrorListener * pMessageListener)
{
    _ASSERT(len > 0  &&  TestFlag(fParseGaps));

    if (m_CurrentGapLength < m_gapNmin)
    {
        // the run of N is too short to be assumed as a gap, put N as is.
        m_SeqData.resize(m_SeqData.size() + m_CurrentGapLength, 'X');
        memset(&m_SeqData.at(m_CurrentPos), m_CurrentGapChar, m_CurrentGapLength);
        m_CurrentPos += m_CurrentGapLength;
        return;
    }

    if (TestFlag(fAligning)) {
        TSeqPos pos = GetCurrentPos(ePosWithGapsAndSegs);
        m_Starts[pos + m_Offset][m_Row] = CFastaAlignmentBuilder::kNoPos;
        m_Offset += len;
        m_Starts[pos + m_Offset][m_Row] = pos;
    } else {
        TSeqPos pos = GetCurrentPos(eRawPos);
        SGap::EKnownSize eKnownSize = SGap::eKnownSize_Yes;
        if (len == m_gap_Unknown_length)
        {
            eKnownSize = SGap::eKnownSize_No;
            //len = 0;
        }
        else
        {
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
        }

        const auto cit = m_GapsizeToLinkageEvidence.find(len);
        const auto& gap_linkage_evidence =
            (cit != m_GapsizeToLinkageEvidence.end()) ?
            cit->second :
            m_DefaultLinkageEvidence;

        TGapRef pGap( new SGap(
            pos, len,
            eKnownSize,
            LineNumber(),
            m_gap_type,
            gap_linkage_evidence));

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
    const TStr& line, ILineErrorListener * pMessageListener)
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
    CSeq_gap::ELinkEvid eLinkEvid = CSeq_gap::eLinkEvid_UnspecifiedOnly;
    set<CLinkage_evidence::EType> setOfLinkageEvidence;

    if (m_gap_type && modKeyValueMultiMap.empty()) // fall back to default values coming from caller
    {
        pGapType = m_gap_type;
        if (!m_GapsizeToLinkageEvidence.empty()) {
            setOfLinkageEvidence = m_GapsizeToLinkageEvidence.begin()->second;
        }
        for (const auto& rec : CSeq_gap::GetNameToGapTypeInfoMap())
        {
            if (rec.second.m_eType == *pGapType)
            {
                eLinkEvid = rec.second.m_eLinkEvid;
            }
        }
    }

    ITERATE(TModKeyValueMultiMap, modKeyValue_it, modKeyValueMultiMap) {
        const TStr & sKey   = modKeyValue_it->first;
        const TStr & sValue = modKeyValue_it->second;

        string canonicalKey = CanonicalizeString(sKey);
        if(  canonicalKey == "gap-type") {

            const CSeq_gap::SGapTypeInfo *pGapTypeInfo = CSeq_gap::NameToGapTypeInfo(sValue);
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
            continue;

        }

        if( canonicalKey == "linkage-evidence") {
            // could be semi-colon separated
            vector<CTempString> arrLinkageEvidences;
            NStr::Split(sValue, ";", arrLinkageEvidences,
                        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

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
            continue;
        }

        // unknown mod.
        FASTA_WARNING_EX(
            LineNumber(),
            "Unknown gap modifier name(s): " << sKey,
            ILineError::eProblem_UnrecognizedQualifierName,
            "gapline", sKey, kEmptyStr );
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
    case CSeq_gap::eLinkEvid_UnspecifiedOnly:
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
    case CSeq_gap::eLinkEvid_Forbidden:
        if( ! setOfLinkageEvidence.empty() ) {
            FASTA_WARNING(LineNumber(),
                "FASTA-Reader: This gap-type cannot have any "
                "linkage-evidence specified, so any will be ignored.",
                ILineError::eProblem_ModifierFoundButNoneExpected,
                "gapline" );
            setOfLinkageEvidence.clear();
        }
        break;
    case CSeq_gap::eLinkEvid_Required:
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

void CFastaReader::AssembleSeq(ILineErrorListener * pMessageListener)
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

    if (m_Gaps.empty() && m_SeqData.empty()) {

        _ASSERT(m_TotalGapLength == 0);
            inst.SetLength(0);
            inst.SetRepr(CSeq_inst::eRepr_virtual);
            // empty sequence triggers warning if seq data was expected
            if( ! TestFlag(fDisableNoResidues) &&
                ! TestFlag(fNoSeqData) ) {
                FASTA_ERROR(LineNumber(),
                    "FASTA-Reader: No residues given",
                    CObjReaderParseException::eNoResidues);
            }
    }
    else
    if (m_Gaps.empty() && TestFlag(fNoSplit)) {
        inst.SetLength(GetCurrentPos(eRawPos));
        inst.SetRepr(CSeq_inst::eRepr_raw);
        CRef<CSeq_data> data(new CSeq_data(m_SeqData, format));
        if ( !TestFlag(fLeaveAsText) ) {
            CSeqportUtil::Pack(data, inst.GetLength());
        }
        inst.SetSeq_data(*data);
    } else {
        CDelta_ext& delta_ext = inst.SetExt().SetDelta();
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.SetLength(GetCurrentPos(ePosWithGaps));
        SIZE_TYPE n = m_Gaps.size();

        if (n==0 || m_Gaps[0]->m_uPos > 0)
        {
            TStr chunk(m_SeqData, 0, (n>0 && m_Gaps[0]->m_uPos > 0)?m_Gaps[0]->m_uPos : inst.GetLength());
            delta_ext.AddAndSplit(chunk, format, chunk.length(), false, !TestFlag(fLeaveAsText));
        }

        for (SIZE_TYPE i = 0;  i < n;  ++i) {

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
                            vecLinkEvids.push_back(move(pNewLinkEvid));
                        }
                    }
                }
            }
            delta_ext.Set().push_back(move(gap_ds));

            TSeqPos next_start = (i == n-1) ? m_CurrentPos : m_Gaps[i+1]->m_uPos;
            if (next_start != m_Gaps[i]->m_uPos) {
                TSeqPos seq_len = next_start - m_Gaps[i]->m_uPos;
                TStr chunk(m_SeqData, m_Gaps[i]->m_uPos, seq_len);
                delta_ext.AddAndSplit(chunk, format, chunk.length(), false, !TestFlag(fLeaveAsText));
            }
        }
        if (delta_ext.Get().size() == 1) {
            // simplify -- just one piece
            inst.SetRepr(CSeq_inst::eRepr_raw);
            inst.SetSeq_data(delta_ext.Set().front()
                                ->SetLiteral().SetSeq_data());
            inst.ResetExt();
        }

    }
}

static void s_AddBiomol(CMolInfo::EBiomol biomol, CBioseq& bioseq)
{
    auto pDesc = Ref(new CSeqdesc());
    pDesc->SetMolinfo().SetBiomol(biomol);
    bioseq.SetDescr().Set().emplace_back(move(pDesc));
}

static bool sRefineNaMol(const char* beginSeqData, const char* endSeqData, CBioseq& bioseq)
{
    auto& seqInst = bioseq.SetInst();
    const bool hasT =
        (find_if(beginSeqData, endSeqData, [](char c) { return (c=='t' || c == 'T'); }) != endSeqData);
    const bool hasU =
        (find_if(beginSeqData, endSeqData, [](char c) { return (c=='u' || c == 'U'); }) != endSeqData);

    if (hasT && !hasU) {
        seqInst.SetMol(CSeq_inst::eMol_dna);
        s_AddBiomol(CMolInfo::eBiomol_genomic, bioseq); // RW-931
        return true;
    }

    if (hasU && !hasT) {
        seqInst.SetMol(CSeq_inst::eMol_rna);
        return true;
    }

    return false;
}


void CFastaReader::AssignMolType(ILineErrorListener * pMessageListener)
{
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

    auto& inst = m_CurrentSeq->SetInst();

    if (TestFlag(fForceType)) {
        _ASSERT(default_mol != CSeq_inst::eMol_not_set);
        inst.SetMol(default_mol);
        return;
    }

    if (inst.IsSetMol()) {
        if (inst.GetMol() == CSeq_inst::eMol_na && !m_SeqData.empty()) {
            sRefineNaMol(m_SeqData.data(),
                    m_SeqData.data() + min(m_SeqData.length(), SIZE_TYPE(4096)),
                    *m_CurrentSeq);
        }
        return;
    }

    if (m_SeqData.empty()) {
        // Nothing else to go on, but that's OK (no sequence to worry
        // about encoding); however, Seq-inst.mol is still mandatory.
        inst.SetMol(CSeq_inst::eMol_not_set);
        return;
    }


    // Do the residue frequencies suggest a specific type?
    SIZE_TYPE length = min(m_SeqData.length(), SIZE_TYPE(4096));
    const auto& data = m_SeqData.data();
    switch (CFormatGuess::SequenceType(data, length, strictness)) {
    case CFormatGuess::eNucleotide:
    {
        if (!sRefineNaMol(data, data+length, *m_CurrentSeq)) {
            inst.SetMol(CSeq_inst::eMol_na);
        }
        return;
    }
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
    ILineErrorListener * pMessageListener) const
{

    // check for nuc or aa sequences at the end of the title
    const static size_t kWarnNumNucCharsAtEnd = 20;
    const static size_t kWarnAminoAcidCharsAtEnd = 50;

    const size_t length = sLineText.length();
    SIZE_TYPE pos_to_check = length-1;

    if((length > kWarnNumNucCharsAtEnd) && !TestFlag(fAssumeProt)) {
        // find last non-nuc character, within the last kWarnNumNucCharsAtEnd characters
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
            return true; // found problem
        }
    }

    if((length > kWarnAminoAcidCharsAtEnd) && !TestFlag(fAssumeNuc)) {
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
            return true; // found problem
        }
    }

    return false;
}

CRef<CSeq_entry> CFastaReader::ReadAlignedSet(
    int reference_row, ILineErrorListener * pMessageListener)
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
    ILineErrorListener * pMessageListener)
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
                            CObjReaderParseException::eFormat );
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


CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, CFastaReader::TFlags flags,
                           int* counter, CFastaReader::TMasks* lcv,
                           ILineErrorListener* pMessageListener)
{
    CFastaReader reader(in, flags);
    if (counter) {
        reader.SetIDGenerator().SetCounter(*counter);
    }
    if (lcv) {
        reader.SaveMasks(lcv);
    }

    auto pEntry = reader.ReadSet(kMax_Int, pMessageListener);

    if (counter) {
        *counter = reader.GetIDGenerator().GetCounter();
    }
    return pEntry;
}


IFastaEntryScan::~IFastaEntryScan()
{
}


class CFastaMapper : public CFastaReader
{
public:
    typedef CFastaReader TParent;

    CFastaMapper(ILineReader& reader,
                 SFastaFileMap* fasta_map,
                 TFlags flags,
                 FIdCheck f_idcheck = CSeqIdCheck());

protected:
    void ParseDefLine(const TStr& s,
        ILineErrorListener * pMessageListener);
    void ParseTitle(const SLineTextAndLoc & lineInfo,
        ILineErrorListener * pMessageListener = 0);
    void AssembleSeq(ILineErrorListener * pMessageListener);

private:
    SFastaFileMap*             m_Map;
    SFastaFileMap::SFastaEntry m_MapEntry;
};

CFastaMapper::CFastaMapper(ILineReader& reader,
                           SFastaFileMap* fasta_map,
                           TFlags flags,
                           FIdCheck f_idcheck)
    : TParent(reader, flags, f_idcheck), m_Map(fasta_map)
{
    _ASSERT(fasta_map);
    fasta_map->file_map.resize(0);
}

void CFastaMapper::ParseDefLine(const TStr& s, ILineErrorListener * pMessageListener)
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
    ILineErrorListener * pMessageListener)
{
    TParent::ParseTitle(s, pMessageListener);
    m_MapEntry.description = s.m_sLineText;
}

void CFastaMapper::AssembleSeq(ILineErrorListener * pMessageListener)
{
    TParent::AssembleSeq(pMessageListener);
    m_Map->file_map.push_back(m_MapEntry);
}


void ReadFastaFileMap(SFastaFileMap* fasta_map, CNcbiIfstream& input)
{
    static const CFastaReader::TFlags kFlags
        = CFastaReader::fAssumeNuc | CFastaReader::fNoSeqData;

    if ( !input.is_open() ) {
        return;
    }

    CRef<ILineReader> lr(ILineReader::New(input));
    CFastaMapper      mapper(*lr, fasta_map, kFlags);
    mapper.ReadSet();
}


void ScanFastaFile(IFastaEntryScan* scanner,
                   CNcbiIfstream&   input,
                   CFastaReader::TFlags fread_flags)
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


static void s_AppendMods(
        const CModHandler::TModList& mods,
        string& title
        )
{
    for (const auto& mod : mods) {

        title.append(" ["
                + mod.GetName()
                + "="
                + mod.GetValue()
                + "]");
    }
}


void CFastaReader::SetExcludedMods(const vector<string>& excluded_mods)
{
    m_ModHandler.SetExcludedMods(excluded_mods);
}


void CFastaReader::x_ApplyMods(
     const string& title,
     TSeqPos line_number,
     CBioseq& bioseq,
     ILineErrorListener* pMessageListener )
{
    string processed_title = title;
    if (TestFlag(fAddMods)) {
        string remainder;
        CModHandler::TModList mods;
        CTitleParser::Apply(processed_title, mods, remainder);

        const auto pFirstID = bioseq.GetFirstId();
        _ASSERT(pFirstID != nullptr);
        const auto idString = pFirstID->AsFastaString();

        CDefaultModErrorReporter
            errorReporter(idString, line_number, pMessageListener);

        CModHandler::TModList rejected_mods;
        m_ModHandler.Clear();
        m_ModHandler.AddMods(mods, CModHandler::eReplace, rejected_mods, errorReporter);
        s_AppendMods(rejected_mods, remainder);

        CModHandler::TModList skipped_mods;
        const bool logInfo =
            pMessageListener ?
            pMessageListener->SevEnabled(eDiag_Info) :
            false;
        CModAdder::Apply(m_ModHandler, bioseq, skipped_mods, logInfo, errorReporter);
        s_AppendMods(skipped_mods, remainder);

        processed_title = remainder;
    }
    else
    if (!TestFlag(fIgnoreMods) &&
        CTitleParser::HasMods(title)) {
        FASTA_WARNING(line_number,
        "FASTA-Reader: Ignoring FASTA modifier(s) found because "
        "the input was not expected to have any.",
        ILineError::eProblem_ModifierFoundButNoneExpected,
        "defline");
    }

    NStr::TruncateSpacesInPlace(processed_title);
    if (!processed_title.empty()) {
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetTitle() = processed_title;
        bioseq.SetDescr().Set().push_back(move(pDesc));
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


void CFastaReader::SetGapLinkageEvidence(
    CSeq_gap::EType type,
    const set<int>& defaultEvidence,
    const map<TSeqPos, set<int>>& countToEvidenceMap
)
{
    SetGapLinkageEvidences(type, defaultEvidence);

    m_GapsizeToLinkageEvidence.clear();
    for (const auto& key_val : countToEvidenceMap) {
        const auto& input_evidence_set = key_val.second;
        auto& evidence_set = m_GapsizeToLinkageEvidence[key_val.first];
        for (const auto& evidence : input_evidence_set) {
            evidence_set.insert(static_cast<CLinkage_evidence::EType>(evidence));
        }
    }
}


void CFastaReader::SetGapLinkageEvidences(CSeq_gap::EType type, const set<int>& evidences)
{
    m_gap_type.Reset(new SGap::TGapTypeObj(type));

    m_DefaultLinkageEvidence.clear();
    for (const auto& evidence : evidences) {
        m_DefaultLinkageEvidence.insert(static_cast<CLinkage_evidence::EType>(evidence));
    }
}


void CFastaReader::PostWarning(
            ILineErrorListener * pMessageListener,
            EDiagSev _eSeverity, size_t _uLineNum, CTempString _MessageStrmOps, CObjReaderParseException::EErrCode _eErrCode, ILineError::EProblem _eProblem, CTempString _sFeature, CTempString _sQualName, CTempString _sQualValue) const
{
    if (find(m_ignorable.begin(), m_ignorable.end(), _eProblem) != m_ignorable.end())
        // this is a problem that should be ignored
        return;

    string sSeqId = ( m_BestID ? m_BestID->AsFastaString() : kEmptyStr);
    AutoPtr<CObjReaderLineException> pLineExpt(
        CObjReaderLineException::Create(
        (_eSeverity), _uLineNum,
        _MessageStrmOps,
        (_eProblem),
        sSeqId, (_sFeature),
        (_sQualName), (_sQualValue),
        _eErrCode) );
    if ( ! pMessageListener && (_eSeverity) <= eDiag_Warning ) {
        LOG_POST_X(1, Warning << pLineExpt->Message());
    } else if ( ! pMessageListener || ! pMessageListener->PutError( *pLineExpt ) )
    {
        throw CObjReaderParseException(DIAG_COMPILE_INFO, 0, _eErrCode, _MessageStrmOps, _uLineNum, _eSeverity);
    }
}

void CFastaReader::IgnoreProblem(ILineError::EProblem problem)
{
    m_ignorable.push_back(problem);
}


END_SCOPE(objects)
END_NCBI_SCOPE

