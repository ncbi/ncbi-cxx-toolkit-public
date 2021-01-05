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
#include <objtools/readers/seqid_validate.hpp>

#define NCBI_USE_ERRCODE_X Objtools_Rd_Fasta // Will need to change this 

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

size_t CFastaDeflineReader::s_MaxLocalIDLength = CSeq_id::kMaxLocalIDLength;
size_t CFastaDeflineReader::s_MaxGeneralTagLength = CSeq_id::kMaxGeneralTagLength;
size_t CFastaDeflineReader::s_MaxAccessionLength = CSeq_id::kMaxAccessionLength;



static void s_PostError(ILineErrorListener* pMessageListener,
    const TSeqPos lineNumber,
    const string& idString,
    const string& errMessage,
    const CObjReaderLineException::EProblem problem,
    const CObjReaderParseException::EErrCode errCode) 
{
    
    if (pMessageListener) {
        unique_ptr<CObjReaderLineException> pLineExpt(
            CObjReaderLineException::Create(
            eDiag_Error,
            lineNumber,
            errMessage, 
            problem,
            idString, "", "", "",
            errCode));

        if (pMessageListener->PutError(*pLineExpt)) {
            return;
        }
    }

    throw CObjReaderParseException(DIAG_COMPILE_INFO, 
            0, 
            errCode, 
            errMessage, 
            lineNumber, 
            eDiag_Error);
}

static void s_PostWarning(ILineErrorListener* pMessageListener, 
    const TSeqPos lineNumber,
    const string& idString,
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
        idString, "", "", "",
        errCode));

    if (!pMessageListener) {
        LOG_POST_X(1, Warning << pLineExpt->Message());
        return;
    }

    if (!pMessageListener->PutError(*pLineExpt)) {
        throw CObjReaderParseException(DIAG_COMPILE_INFO, 
                0, 
                errCode, 
                errMessage, 
                lineNumber, 
                eDiag_Warning);
    }
}

// For reasons of efficiency, this method does not use CRef<CSeq_interval> to access range 
// information - RW-26
void CFastaDeflineReader::ParseDefline(const CTempString& defline,
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignored_errors,
    TIds& ids,
    bool& has_range,
    TSeqPos& range_start,
    TSeqPos& range_end,
    TSeqTitles& titles, 
    ILineErrorListener* pMessageListener) 
{
    SDeflineData data;
    ParseDefline(defline, info, data, pMessageListener);
    has_range   = data.has_range;
    range_start = data.range_start;
    range_end   = data.range_end;
    titles  = move(data.titles);
}

void CFastaDeflineReader::ParseDefline(const CTempString& defline,
        const SDeflineParseInfo& info,
        SDeflineData& data,
        ILineErrorListener* pMessageListener)
{
    static CSeqIdCheck fn_idcheck;
    ParseDefline(defline, info, data, pMessageListener, fn_idcheck);
}


void CFastaDeflineReader::ParseDefline(const CTempString& defline,
        const SDeflineParseInfo& info,
        SDeflineData& data,
        ILineErrorListener* pMessageListener,
        FIdCheck fn_idcheck) 
{
    size_t range_len = 0;
    const TFastaFlags& fFastaFlags = info.fFastaFlags;
    const TSeqPos& lineNumber = info.lineNumber;
    data.has_range = false;

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
                                       data.range_start, data.range_end, pMessageListener);
            }

            auto id_string = defline.substr(start, pos - start - range_len);
            if (NStr::IsBlank(id_string)) {
                NCBI_THROW2(CObjReaderParseException, eFormat, 
                    "Unable to locate sequence id in definition line", 0);
            }

        title_start = pos;
        x_ProcessIDs(id_string,
            info,
            data.ids, 
            pMessageListener,
            fn_idcheck);

        data.has_range = (range_len>0);
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
        data.titles.push_back(
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


class CIdErrorReporter
{
public:
    CIdErrorReporter(ILineErrorListener* pMessageListener, bool ignoreGeneralParsingError=false);

    void operator() (EDiagSev severity, 
            int lineNum, 
            const string& idString,
            CFastaIdValidate::EErrCode errCode,
            const string& msg);
private:
    using TCodePair = pair<CObjReaderLineException::EProblem, CObjReaderParseException::EErrCode>;
    ILineErrorListener* m_pMessageListener = nullptr;
    bool m_IgnoreGeneralParsingError=false;
};



CIdErrorReporter::CIdErrorReporter(ILineErrorListener* pMessageListener, bool ignoreGeneralParsingError) : 
    m_pMessageListener(pMessageListener), m_IgnoreGeneralParsingError(ignoreGeneralParsingError) {}


void CIdErrorReporter::operator()(EDiagSev severity,
        int lineNum,
        const string& idString,
        CFastaIdValidate::EErrCode errCode,
        const string& msg)
{

    static map<CFastaIdValidate::EErrCode,TCodePair> s_CodeMap = /* replace with compile-time map */
    {
     {CFastaIdValidate::eIDTooLong,{ILineError::eProblem_GeneralParsingError, CObjReaderParseException::eIDTooLong}},
     {CFastaIdValidate::eBadLocalID,{ILineError::eProblem_GeneralParsingError, CObjReaderParseException::eInvalidID}},
     {CFastaIdValidate::eUnexpectedNucResidues,{ILineError::eProblem_UnexpectedNucResidues, CObjReaderParseException::eFormat}},
     {CFastaIdValidate::eUnexpectedAminoAcids,{ILineError::eProblem_UnexpectedAminoAcids, CObjReaderParseException::eFormat}}
    };


    const auto cit = s_CodeMap.find(errCode);
    _ASSERT(cit != s_CodeMap.end()); // convert this to a compile-time assertion 

    const auto& problem = cit->second.first;
    if (m_IgnoreGeneralParsingError &&
        problem == ILineError::eProblem_GeneralParsingError) {
        return;
    }

    const auto& parseExceptionCode = cit->second.second;
    if (severity == eDiag_Error) {
        s_PostError(m_pMessageListener, lineNum, idString, msg, problem, parseExceptionCode);
    }
    else {
        s_PostWarning(m_pMessageListener, lineNum, idString, msg, problem, parseExceptionCode);
    }
}


void CFastaDeflineReader::x_ProcessIDs(
    const CTempString& id_string,
    const SDeflineParseInfo& info,
    TIds& ids,
    ILineErrorListener* pMessageListener,
    FIdCheck f_id_check
    )
{
    if (info.fBaseFlags & CReaderBase::fAllIdsAsLocal) 
    {
        CRef<CSeq_id> pSeqId(new CSeq_id(CSeq_id::e_Local, id_string));
        ids.push_back(pSeqId);
        f_id_check(ids, info, pMessageListener);
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

        s_PostWarning(pMessageListener,
            info.lineNumber,
            id_string,
            err_message,
            ILineError::eProblem_GeneralParsingError,
            CObjReaderParseException::eFormat);
            
        local_copy = id_string;
        for (auto& rit : local_copy)
            if (rit == ',')
                rit = '_';

        to_parse = local_copy;
    }

    try {
        CSeq_id::ParseIDs(ids, to_parse, flags);
        ids.remove_if([](CRef<CSeq_id> id_ref)
                { return NStr::IsBlank(id_ref->GetSeqIdString()); });
    }
    catch(...) {
        ids.clear();
    }

    if (ids.empty()) {
        s_PostError(pMessageListener, 
                info.lineNumber,
                id_string,
                "Could not construct seq-id from '" + id_string + "'",
                ILineError::eProblem_GeneralParsingError,
                CObjReaderParseException::eNoIDs);
    
        ids.push_back(Ref(new CSeq_id(CSeq_id::e_Local, id_string)));
        return;
    }
    // Convert anything that looks like a GI to a local id
    if ( info.fBaseFlags & CReaderBase::fNumericIdsAsLocal ) {
        x_ConvertNumericToLocal(ids);
    }

    f_id_check(ids, info, pMessageListener);
}


bool CFastaDeflineReader::ParseIDs(
    const CTempString& s,
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors,
    TIds& ids, 
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

    // be generous overall, and give raw local IDs the benefit of the
    // doubt for now
    CSeq_id::TParseFlags flags
        = CSeq_id::fParse_PartialOK | CSeq_id::fParse_AnyLocal;
    if ( info.fFastaFlags & CFastaReader::fParseRawID ) {
        flags |= CSeq_id::fParse_RawText;
    }

    const bool ignoreGeneralParsingError
        = (find(ignoredErrors.cbegin(), ignoredErrors.cend(), ILineError::eProblem_GeneralParsingError) 
           != ignoredErrors.cend()); 

    try {
        if (s.find(',') != NPOS && s.find('|') == NPOS)
        {
            string local_copy = s;
            for (auto& ch : local_copy)
                if (ch == ',')
                    ch = '_';

            CSeq_id::ParseIDs(ids, local_copy, flags);

            const string errMessage = 
                "Near line " + NStr::NumericToString(info.lineNumber) 
                + ", the sequence contains 'comma' symbol and replaced with 'underscore' "
                + "symbol. Please find and correct the sequence id.";

            if (!ignoreGeneralParsingError) {
                s_PostWarning(pMessageListener, 
                            info.lineNumber,
                            s,
                            errMessage,
                            ILineError::eProblem_GeneralParsingError,
                            CObjReaderParseException::eFormat);

            }
        }
        else
        {
            CSeq_id::ParseIDs(ids, s, flags);
        }
    } catch (CSeqIdException&) {
        // swap(ids, old_ids);
    }

    if ( info.fBaseFlags & CReaderBase::fNumericIdsAsLocal ) {
        x_ConvertNumericToLocal(ids);
    }


    CFastaIdValidate idValidate(info.fFastaFlags);
    if (info.maxIdLength) {
        idValidate.SetMaxLocalIDLength(info.maxIdLength);
        idValidate.SetMaxGeneralTagLength(info.maxIdLength);
        idValidate.SetMaxAccessionLength(info.maxIdLength);
    }
    idValidate(ids, info.lineNumber, CIdErrorReporter(pMessageListener, ignoreGeneralParsingError));

    return true;
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


void CSeqIdCheck::operator()(const TIds& ids, 
                             const TInfo& info,
                             ILineErrorListener* listener) 
{
    if (ids.empty()) {
        return;
    }

    CFastaIdValidate s_IdValidate(info.fFastaFlags);
    if (info.maxIdLength) {
        s_IdValidate.SetMaxLocalIDLength(info.maxIdLength);
        s_IdValidate.SetMaxGeneralTagLength(info.maxIdLength);
        s_IdValidate.SetMaxAccessionLength(info.maxIdLength);
    }
    s_IdValidate(ids, info.lineNumber, CIdErrorReporter(listener));
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


