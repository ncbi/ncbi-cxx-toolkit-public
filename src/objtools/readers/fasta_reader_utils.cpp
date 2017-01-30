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
#include <objects/general/Object_id.hpp>
#include <objtools/readers/fasta_reader_utils.hpp>

#define NCBI_USE_ERRCODE_X Objtools_Rd_Fasta // Will need to change this 

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// For reasons of efficiency, this method does not use CRef<CSeq_interval> to access range 
// information - RW-26
void CFastaDeflineReader::ParseDefline(const string& defline,
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors,
    list<CRef<CSeq_id>>& ids,
    bool& hasRange,
    TSeqPos& rangeStart,
    TSeqPos& rangeEnd,
    TSeqTitles& seqTitles, 
    ILineErrorListener* pMessageListener) 
{
    size_t start = 1, pos, len = defline.length(), title_start;
    size_t range_len = 0;
    const TFastaFlags& fFastaFlags = info.fFastaFlags;
    const TSeqPos& lineNumber = info.lineNumber;

    // ignore spaces between '>' and the sequence ID
    for( ; start < len; ++start ) {
        if( ! isspace(defline[start]) ) {
            break;
        }
    }

    do {
        bool has_id = true;
        if ((fFastaFlags & CFastaReader::fNoParseID)) {
            title_start = start;
        } else {
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

                    if( defline[pos] == '=' ) {
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

            //range_len = ParseRange(substr(defline.data() + start, pos - start),
            range_len = ParseRange(defline.substr(start, pos - start),
                rangeStart, rangeEnd, pMessageListener);
            //has_id = ParseIDs(substr(defline.data() + start, pos - start - range_len), 
            has_id = ParseIDs(defline.substr(start, pos - start - range_len), 
                              info,
                              ignoredErrors,
                              ids, 
                              pMessageListener);
            if (has_id  &&  (fFastaFlags & CFastaReader::fAllSeqIds)  &&  defline[pos] == '\1') {
                start = pos + 1;
                continue;
            }
            title_start = pos;
            // trim leading whitespace from title (is this appropriate?)
            while (title_start < len
                   &&  isspace((unsigned char) defline[title_start])) {
                ++title_start;
            }
        }
        for (pos = title_start + 1;  pos < len;  ++pos) {
            if ((unsigned char) defline[pos] < ' ') {
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
            seqTitles.push_back(
                SLineTextAndLoc(
                defline.substr(title_start, pos - title_start), lineNumber));
        }
        start = pos + 1;
    } while ( (fFastaFlags & CFastaReader::fAllSeqIds)  &&  start < len  &&  defline[start - 1] == '\1'
             &&  !range_len);

    hasRange = (range_len>0);
}


TSeqPos CFastaDeflineReader::ParseRange(
    const string& s, 
    TSeqPos& start, 
    TSeqPos& end, 
    ILineErrorListener * pMessageListener)
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



bool CFastaDeflineReader::ParseIDs(
    const string& s, 
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors,
    list<CRef<CSeq_id>>& ids, 
    ILineErrorListener* pMessageListener) 
{
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
            string temp = NStr::Replace(s, ",", "_");
            num_ids = CSeq_id::ParseIDs(ids, temp, flags);

            const string errMessage = 
                "CFastaReader: Near line " + NStr::NumericToString(info.lineNumber) 
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
    
            if (x_ExcessiveSeqDataInTitle(s, info.fFastaFlags)) {
                return false;
            }

            const string errMessage =
                "CFastaReader: Near line " + NStr::NumericToString(info.lineNumber)
                + ", the sequence ID is too long.  Its length is " + NStr::NumericToString(s.length())
                + " but the max length allowed is "+  NStr::NumericToString(info.maxIdLength)
                + ".  Please find and correct all sequence IDs that are too long.";


            if (!ignoreError) {
                x_PostError(pMessageListener, 
                    info.lineNumber,
                    errMessage,
                    CObjReaderParseException::eIDTooLong);
            }
        }
    }

    return true;
}


bool CFastaDeflineReader::x_IsValidLocalID(const CSeq_id& id, TFastaFlags fasta_flags) 
{
    string id_label;
    id.GetLabel(&id_label, 0, CSeq_id::eContent);
    return x_IsValidLocalID(id_label, fasta_flags);
}


bool CFastaDeflineReader::x_IsValidLocalID(const string& id_string,
    const TFastaFlags fasta_flags)
{
    if ( fasta_flags & CFastaReader::fQuickIDCheck) { // check only the first character
        return CSeq_id::IsValidLocalID(id_string.substr(0,1));
    }
    return CSeq_id::IsValidLocalID(id_string);
}


void CFastaDeflineReader::x_ConvertNumericToLocal(
    list<CRef<CSeq_id>>& ids)
{
    for (auto id : ids) {
        if (id->IsGi()) {
            const TGi gi = id->GetGi();
            id->SetLocal().SetStr(NStr::NumericToString(gi));
        }
    }
}


void CFastaDeflineReader::x_PostWarning(ILineErrorListener* pMessageListener, 
    const TSeqPos lineNumber,
    const string& errMessage, 
    const CObjReaderParseException::EErrCode errCode) 
{
    unique_ptr<CObjReaderLineException> pLineExpt(
        CObjReaderLineException::Create(
        eDiag_Warning,
        lineNumber,
        errMessage, 
        ILineError::eProblem_GeneralParsingError,
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


void CFastaDeflineReader::x_PostError(ILineErrorListener* pMessageListener,
    const TSeqPos lineNumber,
    const string& errMessage,
    const CObjReaderParseException::EErrCode errCode) 
{

    unique_ptr<CObjReaderLineException> pLineExpt(
        CObjReaderLineException::Create(
        eDiag_Error,
        lineNumber,
        errMessage, 
        ILineError::eProblem_GeneralParsingError,
        "", "", "", "",
        errCode));

    if (!pMessageListener || !pMessageListener->PutError(*pLineExpt)) {
        throw CObjReaderParseException(DIAG_COMPILE_INFO, 0, errCode, errMessage, lineNumber, eDiag_Error);
    }
}


bool 
CFastaDeflineReader::x_ExceedsMaxLength(const string& title,
    const TSeqPos max_length)
{
    TSeqPos last = title.rfind('|');
    string substring = (last == NPOS) ? title : title.substr(last+1);

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


bool 
CFastaDeflineReader::x_ExcessiveSeqDataInTitle(const string& title, 
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
        for (auto rit=title.crbegin(); rit!=title.crend(); ++rit) {
            if (!s_ASCII_IsUnAmbigNuc(*rit)) {
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
        for (auto rit=title.crbegin(); rit!=title.crend(); ++rit) {
            const auto ch = *rit;
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
    const bool advance = true;
    while (unique_id) {
        auto p_Id = mp_IdGenerator->GenerateID(advance);
        auto idh = CSeq_id_Handle::GetHandle(*p_Id);
        if (x_IsUniqueIdHandle(idh)) {
            return p_Id;
        }
    }
    // !unique_id
    return mp_IdGenerator->GenerateID(advance);
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


END_SCOPE(objects)
END_NCBI_SCOPE


