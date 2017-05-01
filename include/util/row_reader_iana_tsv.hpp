#ifndef UTIL___ROW_READER_IANA_TSV__HPP
#define UTIL___ROW_READER_IANA_TSV__HPP

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
* Authors: Denis Vakatov, Sergey Satskiy
*
* File Description:
*   Implementation of the CRowReader<> traits for IANA TSV
*   https://www.iana.org/assignments/media-types/text/tab-separated-values
*
* ===========================================================================
*/

#include <util/row_reader_char_delimited.hpp>


BEGIN_NCBI_SCOPE


// Note 1: IANA TSV requires to have the same number of fields in each row.
//         So an exception will be generated if a mismatch is detected in both
//         cases:
//          - validation of the source
//          - iterating over the source
// Note 2: The row with names does not appear in the iteration loop


/// Exception specific to IANA TSV sources
class CCRowReaderStream_IANA_TSV_Exception : public CException
{
public:
    enum EErrCode {
        eNumberOfFieldsMismatch = 1
    };

    virtual const char * GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
            case eNumberOfFieldsMismatch:
                return "eNumberOfFieldsMismatch";
            default:
                return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCRowReaderStream_IANA_TSV_Exception, CException);
};



/// IANA TSV traits.
/// It follows the specs at:
/// https://www.iana.org/assignments/media-types/text/tab-separated-values
class CRowReaderStream_IANA_TSV : public TRowReaderStream_SingleTabDelimited
{
public:
    CRowReaderStream_IANA_TSV() :
        m_FieldNamesExtracted(false), m_NumberOfFields(0)
    {}

    ERR_Action Tokenize(const CTempString    raw_line,
                        vector<CTempString>& tokens)
    {
        if (!m_FieldNamesExtracted) {
            x_ExtractNames(raw_line);
            return eRR_Skip;
        }

        // The field names have been extracted so tokenize and check the data
        // consistency.
        // Note: the only action Tokenize() returns is eRR_Continue_Data
        TRowReaderStream_SingleTabDelimited::Tokenize(raw_line, tokens);

        if (tokens.size() != m_NumberOfFields)
            NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                       eNumberOfFieldsMismatch,
                       "Unexpected number of fields. The first line declared "
                       + NStr::NumericToString(m_NumberOfFields) +
                       "fields while the current line has " +
                       NStr::NumericToString(tokens.size()) + " fields");

        return eRR_Continue_Data;
    }

    ERR_Action Validate(CTempString raw_line)
    {
        m_Tokens.clear();
        this->Tokenize(raw_line, m_Tokens);
        return eRR_Skip;
    }

    ERR_EventAction OnEvent(ERR_Event event)
    {
        switch (event) {
            case eRR_Event_SourceSwitch:
                GetMyStream().ClearFieldsInfo();
                m_FieldNamesExtracted = false;
                m_NumberOfFields = 0;
                return eRR_EventAction_Continue;

            case eRR_Event_SourceEOF:
            case eRR_Event_SourceError:
            default:
                ;
        }
        return eRR_EventAction_Default;
    }

private:
    bool                    m_FieldNamesExtracted;
    TFieldNo                m_NumberOfFields;
    vector<CTempString>     m_Tokens;

    void x_ExtractNames(const CTempString& raw_line)
    {
        m_Tokens.clear();
        TRowReaderStream_SingleTabDelimited::Tokenize(raw_line, m_Tokens);
        for (const auto&  name : m_Tokens) {
            GetMyStream().SetFieldName(m_NumberOfFields,
                                       string(name.data(), name.length()));
            ++m_NumberOfFields;
        }
        m_FieldNamesExtracted = true;
    }

    RR_TRAITS_PARENT_STREAM(CRowReaderStream_IANA_TSV);
};


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_IANA_TSV__HPP */
