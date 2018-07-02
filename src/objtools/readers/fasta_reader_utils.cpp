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
* Authors:  Justin Foley
*
* File Description:
*   Reader for FASTA-format definition lines. Based on code 
*   originally contained in CFastaReader.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/fasta_reader_utils.hpp>

#define NCBI_USE_ERRCODE_X Objtools_Rd_Fasta // Will need to change this 

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

size_t CFastaDeflineReader::s_MaxLocalIDLength = CSeq_id::kMaxLocalIDLength;
size_t CFastaDeflineReader::s_MaxGeneralTagLength = CSeq_id::kMaxGeneralTagLength;
size_t CFastaDeflineReader::s_MaxAccessionLength = CSeq_id::kMaxAccessionLength;

class CSeqIdChecker
{
public:
    using TInfo = CFastaDeflineReader::SDeflineParseInfo;

    CSeqIdChecker(const TInfo& info,
                  ILineErrorListener* pMessageListener) : 
        m_Info(info),
        m_pMessageListener(pMessageListener) {}

    virtual void operator()(const list<CRef<CSeq_id>>& ids);

protected:
    virtual bool x_IsValidLocalID(const CSeq_id& id);
    virtual void x_CheckIDLength(const CSeq_id& id);
    virtual void x_CheckForExcessiveSeqData(const CSeq_id& id);
    virtual void x_PostIDLengthError(const size_t id_length,
                                     const string& type_string,
                                     const size_t max_length);

    const TInfo& m_Info;
    ILineErrorListener* m_pMessageListener;
};

// For reasons of efficiency, this method does not use CRef<CSeq_interval> to access range 
// information - RW-26
void CFastaDeflineReader::ParseDefline(const CTempString& defline,
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors,
    list<CRef<CSeq_id>>& ids,
    bool& hasRange,
    TSeqPos& rangeStart,
    TSeqPos& rangeEnd,
    TSeqTitles& seqTitles, 
    ILineErrorListener* pMessageListener) 
{
    size_t range_len = 0;
    const TFastaFlags& fFastaFlags = info.fFastaFlags;
    const TSeqPos& lineNumber = info.lineNumber;
    hasRange = false;

    const size_t len = defline.length();
    if (len <= 1 || 
        NStr::IsBlank(defline.substr(1))) {
        return;
    }

    if (defline[0] != '>') {
        NCBI_THROW2(CObjReaderParseException, eFormat, 
            "Invalid defline. First character is not '>'", 0);
    }


    // ignore spaces between '>' and the sequence ID
    size_t start;
    for(start = 1 ; start < len; ++start ) {
        if( ! isspace(defline[start]) ) {
            break;
        }
    }

    size_t pos;
    size_t title_start = NPOS;
    if ((fFastaFlags & CFastaReader::fNoParseID)) {
        title_start = start;
    }
    else 
    {
    // This loop finds the end of the sequence ID
        for ( pos = start;  pos < len;  ++pos) {
            unsigned char c = defline[pos];
                
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
                // reaches bracket_give_up_pos, we can still say defline[pos] without worrying
                // about array-out-of-bounds issues.
                const size_t bracket_give_up_pos = min(len - 1, kMaxCharsToLookAt);
                // keep track of the first space we find, because that becomes the end of the seqid
                // if this turns out not to be a FASTA mod.
                size_t first_space_pos = kMax_UI4;

                // find the end of the key
                for( ; pos < bracket_give_up_pos ; ++pos ) {
                    const unsigned char c = defline[pos];
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

                if(defline[pos] == '=' ) {
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


        if ( ! (fFastaFlags & CFastaReader::fDisableParseRange) ) {
            range_len = ParseRange(defline.substr(start, pos - start),
                                   rangeStart, rangeEnd, pMessageListener);
        }

        auto id_string = defline.substr(start, pos - start - range_len);
        if (NStr::IsBlank(id_string)) {
            NCBI_THROW2(CObjReaderParseException, eFormat, 
                "Unable to locate sequence id in definition line", 0);
        }

        title_start = pos;
        x_ProcessIDs(id_string,
            info,
            ids, 
            pMessageListener,
            CSeqIdChecker(info, pMessageListener));

        hasRange = (range_len>0);
    }

    
    // trim leading whitespace from title (is this appropriate?)
    while (title_start < len
        &&  isspace((unsigned char)defline[title_start])) {
        ++title_start;
    }

    if (title_start < len) { 
        for (pos = title_start + 1;  pos < len;  ++pos) {
            if ((unsigned char)defline[pos] < ' ') {
            break;
            }
        }
        // Parse the title elsewhere - after the molecule has been deduced
        seqTitles.push_back(
            SLineTextAndLoc(
                defline.substr(title_start, pos - title_start), lineNumber));
    }
}


TSeqPos CFastaDeflineReader::ParseRange(
    const CTempString& s, 
    TSeqPos& start, 
    TSeqPos& end, 
    ILineErrorListener * pMessageListener)
{

    if (s.empty()) {
        return 0;
    }

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
    return TSeqPos(s.length() - pos);
}



void CFastaDeflineReader::x_PostIDLengthError(const size_t id_length,
                                             const string& type_string,
                                             const size_t max_length,
                                             const int line_number,
                                             ILineErrorListener* pMessageListener)

{
    const string& err_message =
        "Near line " + NStr::NumericToString(line_number) +
        + ", the " + type_string + " is too long.  Its length is " + NStr::NumericToString(id_length)
        + " but the maximum allowed " + type_string + " length is "+  NStr::NumericToString(max_length)
        + ".  Please find and correct all " + type_string + "s that are too long.";

    x_PostError(pMessageListener, 
        line_number,
        err_message,
        CObjReaderParseException::eIDTooLong);
}



void CFastaDeflineReader::x_CheckIDLength(
        const CSeq_id& seq_id, 
        const int line_number,
        ILineErrorListener* pMessageListener)
{

    if (seq_id.IsLocal()) {
        if (seq_id.GetLocal().IsStr() &&
            seq_id.GetLocal().GetStr().length() > s_MaxLocalIDLength) {
            x_PostIDLengthError(seq_id.GetLocal().GetStr().length(),
                                "local id",
                                s_MaxLocalIDLength,
                                line_number,
                                pMessageListener);
        }
        return;
    }


    if (seq_id.IsGeneral()) {
        if (seq_id.GetGeneral().IsSetTag() &&
            seq_id.GetGeneral().GetTag().IsStr()) {
            const auto length = seq_id.GetGeneral().GetTag().GetStr().length();
            if (length > s_MaxGeneralTagLength) {
                x_PostIDLengthError(length,
                                    "general id string",
                                    s_MaxGeneralTagLength,
                                    line_number,
                                    pMessageListener);
            }
        }
        return;
    }


   auto pTextId = seq_id.GetTextseq_Id();
   if (pTextId &&
       pTextId->IsSetAccession()) {
        const auto length = pTextId->GetAccession().length();
        if (length > s_MaxAccessionLength) {
            x_PostIDLengthError(length,
                                "accession",
                                s_MaxAccessionLength,
                                line_number,
                                pMessageListener);
        }
   }
}
        


void CFastaDeflineReader::x_ProcessIDs(
    const CTempString& id_string,
    const SDeflineParseInfo& info,
    list<CRef<CSeq_id>>& ids,
    ILineErrorListener* pMessageListener,
    function<void(const list<CRef<CSeq_id>>&)> f_id_checker)
{
    if (info.fBaseFlags & CReaderBase::fAllIdsAsLocal) 
    {
        CRef<CSeq_id> pSeqId(new CSeq_id(CSeq_id::e_Local, id_string));
        ids.push_back(pSeqId);
        f_id_checker(ids);
        return;
    }

    CSeq_id::TParseFlags flags = 
        CSeq_id::fParse_PartialOK |
        CSeq_id::fParse_AnyLocal;

    if (info.fFastaFlags & CFastaReader::fParseRawID) {
        flags |= CSeq_id::fParse_RawText;
    }

    string local_copy;
    auto to_parse = id_string;
    if (id_string.find(',') != NPOS &&
        id_string.find('|') == NPOS) {
            
        const string err_message = 
            "Near line " + NStr::NumericToString(info.lineNumber)
            + ", the sequence id string contains 'comma' symbol, which has been replaced with 'underscore' "
            + "symbol. Please correct the sequence id string.";

        x_PostWarning(pMessageListener,
            info.lineNumber,
            err_message,
            CObjReaderParseException::eFormat);
            
        local_copy = id_string;
        for (auto& rit : local_copy)
            if (rit == ',')
                rit = '_';

        to_parse = local_copy;
    }

    CSeq_id::ParseIDs(ids, to_parse, flags);

    ids.remove_if([](CRef<CSeq_id> id_ref){ return NStr::IsBlank(id_ref->GetSeqIdString()); });
    if (ids.empty()) {
        NCBI_THROW2(CObjReaderParseException, eNoIDs,
                "Could not construct seq-id from '" + id_string + "'",
                0);
    }
    // Convert anything that looks like a GI to a local id
    if ( info.fBaseFlags & CReaderBase::fNumericIdsAsLocal ) {
        x_ConvertNumericToLocal(ids);
    }

    f_id_checker(ids);
}


bool CFastaDeflineReader::ParseIDs(
    const CTempString& s,
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors,
    list<CRef<CSeq_id>>& ids, 
    ILineErrorListener* pMessageListener) 
{
    if (s.empty()) {
        return false;
    }

    // if user wants all ids to be purely local, no problem
    if( info.fBaseFlags & CReaderBase::fAllIdsAsLocal )
    {
        ids.push_back(Ref(new CSeq_id(CSeq_id::e_Local, s)));
        return true;
    }

    TSeqPos num_ids = 0;
    // be generous overall, and give raw local IDs the benefit of the
    // doubt for now
    CSeq_id::TParseFlags flags
        = CSeq_id::fParse_PartialOK | CSeq_id::fParse_AnyLocal;
    if ( info.fFastaFlags & CFastaReader::fParseRawID ) {
        flags |= CSeq_id::fParse_RawText;
    }

    const bool ignoreError
        = (find(ignoredErrors.cbegin(), ignoredErrors.cend(), ILineError::eProblem_GeneralParsingError) 
           != ignoredErrors.cend()); 

    try {
        if (s.find(',') != NPOS && s.find('|') == NPOS)
        {
            string local_copy = s;
            for (auto& ch : local_copy)
                if (ch == ',')
                    ch = '_';
            num_ids = CSeq_id::ParseIDs(ids, local_copy, flags);

            const string errMessage = 
                "Near line " + NStr::NumericToString(info.lineNumber) 
                + ", the sequence contains 'comma' symbol and replaced with 'underscore' "
                + "symbol. Please find and correct the sequence id.";

            if (!ignoreError) {
                x_PostWarning(pMessageListener, 
                            info.lineNumber,
                            errMessage,
                            CObjReaderParseException::eFormat);

            }
        }
        else
        {
            num_ids = CSeq_id::ParseIDs(ids, s, flags);
        }
    } catch (CSeqIdException&) {
        // swap(ids, old_ids);
    }


    
    if ( info.fBaseFlags & CReaderBase::fNumericIdsAsLocal ) {
        x_ConvertNumericToLocal(ids);
    }

    if (num_ids == 1) {
        if (ids.front()->IsLocal()) 
        {
            if (!x_IsValidLocalID(*ids.front(), info.fFastaFlags)) {
                ids.clear();
                return false;
            }
        }
        if ( x_ExceedsMaxLength(s, info.maxIdLength) ) {
            const string errMessage =
                "Near line " + NStr::NumericToString(info.lineNumber)
                + ", the sequence ID is too long.  Its length is " + NStr::NumericToString(s.length())
                + " but the max length allowed is "+  NStr::NumericToString(info.maxIdLength)
                + ".  Please find and correct all sequence IDs that are too long.";


            if (!ignoreError) {
                x_PostError(pMessageListener, 
                    info.lineNumber,
                    errMessage,
                    CObjReaderParseException::eIDTooLong);
            }

            if (x_ExcessiveSeqDataInTitle(s, info.fFastaFlags)) {
                return false;
            }
        }
    }

    return true;
}


bool CFastaDeflineReader::x_IsValidLocalID(const CSeq_id& id, TFastaFlags fasta_flags) 
{
    if (id.IsLocal() && id.GetLocal().IsStr())
       return x_IsValidLocalID(id.GetLocal().GetStr(), fasta_flags);

    return true;
}


bool CFastaDeflineReader::x_IsValidLocalID(const CTempString& id_string,
    const TFastaFlags fasta_flags)
{
    auto string_to_check = (fasta_flags & CFastaReader::fQuickIDCheck)  ?
                                    id_string.substr(0,1) :
                                    id_string;

    return !(CSeq_id::CheckLocalID(string_to_check)&CSeq_id::fInvalidChar);
}


void CFastaDeflineReader::x_ConvertNumericToLocal(
    list<CRef<CSeq_id>>& ids)
{
    for (auto id : ids) {
        if (id->IsGi()) {
            const TGi gi = id->GetGi();
            id->SetLocal().SetStr() = NStr::NumericToString(gi);
        }
    }
}


static void s_PostWarning(ILineErrorListener* pMessageListener, 
    const TSeqPos lineNumber,
    const string& errMessage, 
    const CObjReaderLineException::EProblem problem, 
    const CObjReaderParseException::EErrCode errCode) 
{
    unique_ptr<CObjReaderLineException> pLineExpt(
        CObjReaderLineException::Create(
        eDiag_Warning,
        lineNumber,
        errMessage, 
        problem,
        "", "", "", "",
        errCode));

    if (!pMessageListener) {
        LOG_POST_X(1, Warning << pLineExpt->Message());
        return;
    }

    if (!pMessageListener->PutError(*pLineExpt)) {
        throw CObjReaderParseException(DIAG_COMPILE_INFO, 0, errCode, errMessage, lineNumber, eDiag_Warning);
    }
}


void CFastaDeflineReader::x_PostWarning(ILineErrorListener* pMessageListener,
    const TSeqPos lineNumber,
    const string& errMessage,
    const CObjReaderParseException::EErrCode errCode) 
{
    x_PostWarning(pMessageListener,
        lineNumber,
        errMessage,
        ILineError::eProblem_GeneralParsingError,
        errCode);
}


void CFastaDeflineReader::x_PostWarning(ILineErrorListener* pMessageListener, 
    const TSeqPos lineNumber,
    const string& errMessage, 
    const CObjReaderLineException::EProblem problem, 
    const CObjReaderParseException::EErrCode errCode) 
{
    s_PostWarning(pMessageListener,
                  lineNumber,
                  errMessage,
                  problem,
                  errCode);
}


static void s_PostError(ILineErrorListener* pMessageListener,
                 const TSeqPos lineNumber,
                 const string& errMessage,
                 const CObjReaderLineException::EProblem problem,
                 const CObjReaderParseException::EErrCode errCode) 
{

    unique_ptr<CObjReaderLineException> pLineExpt(
        CObjReaderLineException::Create(
        eDiag_Error,
        lineNumber,
        errMessage, 
        problem,
        "", "", "", "",
        errCode));

    if (!pMessageListener || !pMessageListener->PutError(*pLineExpt)) {
        throw CObjReaderParseException(DIAG_COMPILE_INFO, 0, errCode, errMessage, lineNumber, eDiag_Error);
    }
}


void CFastaDeflineReader::x_PostError(ILineErrorListener* pMessageListener,
    const TSeqPos lineNumber,
    const string& errMessage,
    const CObjReaderParseException::EErrCode errCode) 
{
    x_PostError(pMessageListener,
                lineNumber,
                errMessage,
                ILineError::eProblem_GeneralParsingError, 
                errCode);
}


void CFastaDeflineReader::x_PostError(ILineErrorListener* pMessageListener,
    const TSeqPos lineNumber,
    const string& errMessage,
    const CObjReaderLineException::EProblem problem,
    const CObjReaderParseException::EErrCode errCode) 
{
    s_PostError(pMessageListener,
                lineNumber,
                errMessage,
                problem,
                errCode);
}


bool 
CFastaDeflineReader::x_ExceedsMaxLength(const CTempString& title,
    const TSeqPos max_length)
{
    auto last = title.rfind('|');
    auto substring = (last == NPOS) ? title : title.substr(last+1);

    return (substring.length() > max_length);
}


static bool s_ASCII_IsUnAmbigNuc(unsigned char c)
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


void CSeqIdChecker::operator()(const list<CRef<CSeq_id>>& ids) 
{

    if (ids.empty()) {
        return;
    }


    x_CheckForExcessiveSeqData(*(ids.back()));


    for (const auto& pId : ids) {
        if (pId->IsLocal() && 
            !x_IsValidLocalID(*pId)) {
            NCBI_THROW2(CObjReaderParseException, eFormat, 
                "'" + pId->GetSeqIdString() + "' is not a valid local ID", 0);
        }
        x_CheckIDLength(*pId);
    }
}


bool CSeqIdChecker::x_IsValidLocalID(const CSeq_id& id)
{
    if (id.IsLocal()) {
        if (id.GetLocal().IsId()) {
            return true;
        }
        if (id.GetLocal().IsStr()) {
            auto id_string = id.GetLocal().GetStr();
            auto string_to_check = (m_Info.fFastaFlags & CFastaReader::fQuickIDCheck) ?
                                    id_string.substr(0,1) :
                                    id_string;
            return !(CSeq_id::CheckLocalID(string_to_check)&CSeq_id::fInvalidChar);
        }
    }

    return false;
}


void CSeqIdChecker::x_PostIDLengthError(const size_t id_length,
                                        const string& type_string,
                                        const size_t max_length)
{

    const string& err_message =
        "Near line " + NStr::NumericToString(m_Info.lineNumber) +
        + ", the " + type_string + " is too long.  Its length is " + NStr::NumericToString(id_length)
        + " but the maximum allowed " + type_string + " length is "+  NStr::NumericToString(max_length)
        + ".  Please find and correct all " + type_string + "s that are too long.";

    s_PostError(m_pMessageListener, 
        m_Info.lineNumber,
        err_message,
        ILineError::eProblem_GeneralParsingError,
        CObjReaderParseException::eIDTooLong);

}


void CSeqIdChecker::x_CheckIDLength(const CSeq_id& id) 
{
    if (id.IsLocal()) {
        if (id.GetLocal().IsStr() &&
            id.GetLocal().GetStr().length() > CFastaDeflineReader::s_MaxLocalIDLength) {
            x_PostIDLengthError(id.GetLocal().GetStr().length(),
                                "local id",
                                CFastaDeflineReader::s_MaxLocalIDLength);
        }
        return;
    }


    if (id.IsGeneral()) {
        if (id.GetGeneral().IsSetTag() &&
            id.GetGeneral().GetTag().IsStr()) {
            const auto length = id.GetGeneral().GetTag().GetStr().length();
            if (length > CFastaDeflineReader::s_MaxGeneralTagLength) {
                x_PostIDLengthError(length,
                                    "general id string",
                                    CFastaDeflineReader::s_MaxGeneralTagLength);
            }
        }
        return;
    }


   auto pTextId = id.GetTextseq_Id();
   if (pTextId &&
       pTextId->IsSetAccession()) {
        const auto length = pTextId->GetAccession().length();
        if (length > CFastaDeflineReader::s_MaxAccessionLength) {
            x_PostIDLengthError(length,
                                "accession",
                                CFastaDeflineReader::s_MaxAccessionLength);
        }
   }
}


void CSeqIdChecker::x_CheckForExcessiveSeqData(const CSeq_id& id)
{
    const auto& id_string = id.GetSeqIdString();

    const TSeqPos kWarnNumNucCharsAtEnd = 20;
    const TSeqPos kWarnNumAminoAcidCharsAtEnd = 50;
    const TSeqPos kErrNumNucCharsAtEnd = 25;

    const bool assume_prot = (m_Info.fFastaFlags & CFastaReader::fAssumeProt);

    if (!assume_prot && id_string.length() > kWarnNumNucCharsAtEnd) {
        TSeqPos numNucChars = 0;
        for (size_t i = id_string.size(); i>0; i--) {
            const auto ch = id_string[i - 1];
            if (!s_ASCII_IsUnAmbigNuc(ch) && (ch != 'N')) {
                break;
            }
            ++numNucChars;
        }

        if (numNucChars > kWarnNumNucCharsAtEnd) {
            const string err_message = 
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numNucChars) +
            " valid nucleotide characters. " +
            " Was the sequence accidentally placed in the definition line?";    
       
            if (numNucChars > kErrNumNucCharsAtEnd) {
                s_PostError(m_pMessageListener,
                            m_Info.lineNumber,
                            err_message,
                            ILineError::eProblem_UnexpectedNucResidues,
                            CObjReaderParseException::eFormat);
            
            } else{ 
                s_PostWarning(m_pMessageListener,
                              m_Info.lineNumber,
                              err_message,
                              ILineError::eProblem_UnexpectedNucResidues,
                              CObjReaderParseException::eFormat);
            }
            return;        
        }
    }

    const bool assume_nuc = (m_Info.fFastaFlags & CFastaReader::fAssumeNuc);
    // Check for Aa sequence
    if (!assume_nuc && id_string.length() > kWarnNumAminoAcidCharsAtEnd) {
        TSeqPos numAaChars = 0;
        for (size_t i = id_string.size(); i>0; i--) {
            const auto ch = id_string[i - 1];
            if ( !(ch >= 'A' && ch <= 'Z')  &&
                 !(ch >= 'a' && ch <= 'z') ) {
                break;
            }
            ++numAaChars;
        }
        if (numAaChars > kWarnNumAminoAcidCharsAtEnd) {
            const string err_message = 
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numAaChars) +
            " valid amino-acid characters. " +
            " Was the sequence accidentally placed in the definition line?";    
        
            s_PostWarning(m_pMessageListener,
                          m_Info.lineNumber,
                          err_message,
                          ILineError::eProblem_UnexpectedAminoAcids,
                          CObjReaderParseException::eFormat);
        }
    }
}


void 
CFastaDeflineReader::x_CheckForExcessiveSeqDataInID(
    const CTempString& id_string,
    const SDeflineParseInfo& info,
    ILineErrorListener* pMessageListener) 
{
    const TSeqPos kWarnNumNucCharsAtEnd = 20;
    const TSeqPos kWarnNumAminoAcidCharsAtEnd = 50;
    const TSeqPos kErrNumNucCharsAtEnd = 25;

    const bool assume_prot = (info.fFastaFlags & CFastaReader::fAssumeProt);

    if (!assume_prot && id_string.length() > kWarnNumNucCharsAtEnd) {
        TSeqPos numNucChars = 0;
        for (size_t i = id_string.size(); i>0; i--) {
            const auto ch = id_string[i - 1];
            if (!s_ASCII_IsUnAmbigNuc(ch) && (ch != 'N')) {
                break;
            }
            ++numNucChars;
        }

        if (numNucChars > kWarnNumNucCharsAtEnd) {
            const string err_message = 
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numNucChars) +
            " valid nucleotide characters. " +
            " Was the sequence accidentally placed in the definition line?";    
       
            if (numNucChars > kErrNumNucCharsAtEnd) {
                x_PostError(pMessageListener,
                            info.lineNumber,
                            err_message,
                            ILineError::eProblem_UnexpectedNucResidues,
                            CObjReaderParseException::eFormat);
            
            } else{ 
                x_PostWarning(pMessageListener,
                              info.lineNumber,
                              err_message,
                              ILineError::eProblem_UnexpectedNucResidues,
                              CObjReaderParseException::eFormat);
            }
            return;        
        }
    }

    const bool assume_nuc = (info.fFastaFlags & CFastaReader::fAssumeNuc);
    // Check for Aa sequence
    if (!assume_nuc && id_string.length() > kWarnNumAminoAcidCharsAtEnd) {
        TSeqPos numAaChars = 0;
        for (size_t i = id_string.size(); i>0; i--) {
            const auto ch = id_string[i - 1];
            if ( !(ch >= 'A' && ch <= 'Z')  &&
                 !(ch >= 'a' && ch <= 'z') ) {
                break;
            }
            ++numAaChars;
        }
        if (numAaChars > kWarnNumAminoAcidCharsAtEnd) {
            const string err_message = 
            "Fasta Reader: sequence id ends with " +
            NStr::NumericToString(numAaChars) +
            " valid amino-acid characters. " +
            " Was the sequence accidentally placed in the definition line?";    
        
            x_PostWarning(pMessageListener,
                info.lineNumber,
                err_message,
                ILineError::eProblem_UnexpectedAminoAcids,
                CObjReaderParseException::eFormat);
        }
    }
}



bool 
CFastaDeflineReader::x_ExcessiveSeqDataInTitle(const CTempString& title, 
    TFastaFlags fasta_flags) 
{
    if (fasta_flags & CFastaReader::fAssumeProt) {
        return false;
    }

    // Check for nuc or aa sequence at the end of the title
    const TSeqPos kWarnNumNucCharsAtEnd = 20;
    const TSeqPos kWarnNumAminoAcidCharsAtEnd = 50;

    // Check for nuc sequence
    if (title.length() > kWarnNumNucCharsAtEnd) {
        TSeqPos numNucChars = 0;
        for (size_t i = title.size(); i>0; i--) {
            const auto ch = title[i-1];
            if (!s_ASCII_IsUnAmbigNuc(ch) && (ch != 'N')) {
                break;
            }
            ++numNucChars;
        }
        if (numNucChars > kWarnNumNucCharsAtEnd) {
            return true;
        }
    }

    // Check for Aa sequence
    if (title.length() > kWarnNumAminoAcidCharsAtEnd) {
        TSeqPos numAaChars = 0;
        for (size_t i=title.size(); i>0; i--) {
            const auto ch = title[i-1];
            if ( !(ch >= 'A' && ch <= 'Z')  &&
                 !(ch >= 'a' && ch <= 'z') ) {
                break;
            }
            ++numAaChars;
        }
        if (numAaChars > kWarnNumAminoAcidCharsAtEnd) {
            return true;
        }
    }
    return false;
}


CRef<CSeq_id> CFastaIdHandler::GenerateID(bool unique_id) 
{
    return GenerateID("", unique_id);
}



CRef<CSeq_id> CFastaIdHandler::GenerateID(const string& defline, const bool unique_id)
{
    const bool advance = true;
    while (unique_id) {
        auto p_Id = mp_IdGenerator->GenerateID(defline, advance);
        auto idh = CSeq_id_Handle::GetHandle(*p_Id);
        if (x_IsUniqueIdHandle(idh)) {
            return p_Id;
        }
    }
    // !unique_id
    return mp_IdGenerator->GenerateID(defline, advance);
}


CRef<CSeq_id> CSeqIdGenerator::GenerateID(const bool advance)
{
    return GenerateID("", advance);
}


CRef<CSeq_id> CSeqIdGenerator::GenerateID(const string& defline, const bool advance)
{
    CRef<CSeq_id> seq_id(new CSeq_id);
    auto n = m_Counter.load();
    if (advance)
        m_Counter++;

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



END_SCOPE(objects)
END_NCBI_SCOPE


