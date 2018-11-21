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


const size_t    kReadBufferSize = 128 * 1024;


// Note 1: The row with names does not appear in the iteration loop
// Note 2: IANA TSV has two validation modes:
//  - default; matches source reading iterations requirements
//  - strict; follows the BNF strictly
//
// Check                          | Strict     | Default validation/
//                                | validation | reading iteraitons
// -------------------------------------------------------------------
// empty source                   | exc        | OK
// empty lines                    | exc        | exc
// 0 data records                 | exc        | OK
// empty field names              | exc        | exc
// empty field values             | exc        | exc
// no EOL in the last data record | exc        | OK
//
// 'exc' -> exception is generated
// 'OK' -> no checking performed
//
// Note 3: If the validation is called with the eRR_FieldValidation flag then
// the field basic type information is taken into account. For each provided
// type except eRR_String a conversion is tried. In case of a conversion
// failure an exception is generated.


/// Exception specific to IANA TSV sources
class CCRowReaderStream_IANA_TSV_Exception : public CException
{
public:
    enum EErrCode {
        eNumberOfFieldsMismatch = 1,
        eEmptyDataSource = 2,
        eNoTrailingEOL = 3,
        eEmptyFieldName = 4,
        eEmptyLine = 5,
        eEmptyFieldValue = 6,
        eValidateConversion = 7
    };

    virtual const char * GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
            case eNumberOfFieldsMismatch:
                return "eNumberOfFieldsMismatch";
            case eEmptyDataSource:
                return "eEmptyDataSource";
            case eNoTrailingEOL:
                return "eNoTrailingEOL";
            case eEmptyFieldName:
                return "eEmptyFieldName";
            case eEmptyLine:
                return "eEmptyLine";
            case eEmptyFieldValue:
                return "eEmptyFieldValue";
            case eValidateConversion:
                return "eValidateConversion";
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
    enum ERR_ValidationMode {
        eRR_ValidationMode_Default,
        eRR_ValidationMode_Strict
    };

public:
    CRowReaderStream_IANA_TSV() :
        m_FieldNamesExtracted(false), m_NumberOfFields(0),
        m_ValidationMode(eRR_ValidationMode_Default),
        m_LineHasTrailingEOL(true),
        m_ReadBuffer(new char[kReadBufferSize])
    {}

    ~CRowReaderStream_IANA_TSV()
    {
        delete [] m_ReadBuffer;
    }

    void SetValidationMode(ERR_ValidationMode validation_mode)
    {
        m_ValidationMode = validation_mode;
    }

    // The only reason to have this method re-implemented is to detect if the
    // last line in the stream has a trailing EOL. It makes difference if it is
    // a strict validation mode.
    size_t ReadRowData(CNcbiIstream& is, string* data)
    {
        data->clear();

        // Note: the std::getline() does not affect the read bytes counter so a
        // stream version is used. The stream version uses a character buffer
        streamsize  bytes_read = 0;
        streamsize  total_read = 0;
        for (;;) {
            is.getline(m_ReadBuffer, kReadBufferSize);
            bytes_read = is.gcount();
            total_read += bytes_read;

            if (!is.fail()) {
                // this is presumably the most common case
                break;
            }

            if (is.eof()) {
                if (bytes_read == 0)
                    return 0;
                break;
            }

            // Here: the buffer size is not enough to read the whole line
            data->append(m_ReadBuffer);
            is.clear();
        }

        data->append(m_ReadBuffer);
        m_LineHasTrailingEOL =
            total_read != static_cast<ssize_t>(data->size());
        if(!data->empty()  &&  data->back() == '\r')
            data->pop_back();
        return 1;
    }

    ERR_Action Tokenize(const CTempString    raw_line,
                        vector<CTempString>& tokens)
    {
        if (raw_line.empty())
            NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                       eEmptyLine, "Empty lines are not allowed");

        if (!m_FieldNamesExtracted) {
            x_ExtractNames(raw_line);
            return eRR_Skip;
        }

        // The field names have been extracted so tokenize and check the data
        // consistency.
        // Note: the only action Tokenize() returns is eRR_Continue_Data
        TRowReaderStream_SingleTabDelimited::Tokenize(raw_line, tokens);
        for (const auto&  name : m_Tokens) {
            if (name.empty())
                NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                           eEmptyFieldValue,
                           "Empty field values are not allowed");
        }

        if (tokens.size() != m_NumberOfFields)
            NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                       eNumberOfFieldsMismatch,
                       "Unexpected number of fields. The first line declared "
                       + NStr::NumericToString(m_NumberOfFields) +
                       "fields while the current line has " +
                       NStr::NumericToString(tokens.size()) + " fields");

        return eRR_Continue_Data;
    }

    ERR_Action Validate(CTempString raw_line,
                        ERR_FieldValidationMode field_validation_mode)
    {
        m_Tokens.clear();
        ERR_Action action = this->Tokenize(raw_line, m_Tokens);

        if (m_ValidationMode == eRR_ValidationMode_Strict) {
            if (!m_LineHasTrailingEOL)
                NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                           eNoTrailingEOL,
                           "Strict validation requires "
                           "trailing EOL in the last line as well");
        }

        // Validate field values in accordance with the available
        // basic field types. It is conditional:
        // - the requested validation mode includes value checking
        // - it is not a field names row
        // - the user has provided some type information
        if (field_validation_mode == eRR_FieldValidation &&
            action == eRR_Continue_Data &&
            !m_FieldsToValidate.empty()) {
            for (const auto& info : m_FieldsToValidate)
                if (info.first < m_Tokens.size())
                    CRR_Util::ValidateBasicTypeFieldValue(
                        m_Tokens[info.first],
                        info.second.first, info.second.second);
        }

        return eRR_Skip;
    }

    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode event_mode)
    {
        switch (event) {
            case eRR_Event_SourceBegin:
                GetMyStream().x_ClearTraitsProvidedFieldsInfo();
                m_FieldNamesExtracted = false;
                m_NumberOfFields = 0;

                if (event_mode == eRR_EventMode_Validating)
                    x_GetFieldTypesToValidate();

                return eRR_EventAction_Continue;

            case eRR_Event_SourceEnd:
                if (event_mode == eRR_EventMode_Validating) {
                    if (m_ValidationMode == eRR_ValidationMode_Strict) {
                        if (GetMyStream().GetCurrentLineNo() < 1) {
                            NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                                       eEmptyDataSource,
                                       "Strict validation requires "
                                       "non empty data source with at least "
                                       "two lines (a nameline and a record)");
                        }
                    }
                }
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
    ERR_ValidationMode      m_ValidationMode;
    bool                    m_LineHasTrailingEOL;

    map<size_t, pair<ERR_FieldType, string>> m_FieldsToValidate;

    char*                   m_ReadBuffer;

    void x_ExtractNames(const CTempString& raw_line)
    {
        m_Tokens.clear();
        TRowReaderStream_SingleTabDelimited::Tokenize(raw_line, m_Tokens);
        for (const auto&  name : m_Tokens) {
            if (name.empty())
                NCBI_THROW(CCRowReaderStream_IANA_TSV_Exception,
                           eEmptyFieldName,
                           "Empty field names are not allowed");

            GetMyStream().x_SetFieldName(m_NumberOfFields,
                                         string(name.data(), name.length()));
            ++m_NumberOfFields;
        }
        m_FieldNamesExtracted = true;
    }

    void x_GetFieldTypesToValidate(void)
    {
        m_FieldsToValidate.clear();
        for (const auto& info : GetMyStream().GetFieldsMetaInfo()) {
            if (info.is_type_initialized) {
                auto field_type = info.type.GetType();
                if (field_type == eRR_Boolean || field_type == eRR_Integer ||
                    field_type == eRR_Double || field_type == eRR_DateTime)
                    m_FieldsToValidate[info.field_no] =
                        make_pair(field_type, info.type.GetProps());
            }
        }
    }

    RR_TRAITS_PARENT_STREAM(CRowReaderStream_IANA_TSV);
};


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_IANA_TSV__HPP */
