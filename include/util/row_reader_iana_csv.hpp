#ifndef UTIL___ROW_READER_IANA_CSV__HPP
#define UTIL___ROW_READER_IANA_CSV__HPP

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
*   Implementation of the CRowReader<> traits for IANA CSV
*   https://tools.ietf.org/html/rfc4180
*
* ===========================================================================
*/

#include <util/row_reader_char_delimited.hpp>


BEGIN_NCBI_SCOPE


/// Note 1: Empty rows are allowed and silently skipped
/// Note 2: Both CRLF and LF are allowed
/// Note 3: Number of fields is not enforced
/// Note 4: The row with names does not appear in the iteration loop


/// Exception specific to IANA CSV sources
class CCRowReaderStream_IANA_CSV_Exception : public CException
{
public:
    enum EErrCode {
        eUnbalancedDoubleQuote,
        eUnexpectedDoubleQuote
    };

    virtual const char * GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
            case eUnbalancedDoubleQuote:
                return "eUnbalancedDoubleQuote";
            case eUnexpectedDoubleQuote:
                return "eUnexpectedDoubleQuote";
            default:
                return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCRowReaderStream_IANA_CSV_Exception, CException);
};



/// IANA CSV traits.
/// It follows the specs at:
/// https://tools.ietf.org/html/rfc4180
/// @note by default the source is considered as the one which has a header
///       line. If your source does not features a header then use the
///       CRowReaderStream_IANA_CSV_no_header traits or use the SetHasHeader()
///       member to switch between modes at runtime.
class CRowReaderStream_IANA_CSV : public TRowReaderStream_SingleCommaDelimited
{
public:
    CRowReaderStream_IANA_CSV() :
        m_HasHeader(true)
    {
        m_LineSeparator.reserve(2);
        m_PreviousLineSeparator.reserve(2);
    }

    // It could be more than one raw line in one row
    size_t ReadRowData(CNcbiIstream& is, string* data)
    {
        data->clear();
        m_Tokens.clear();

        size_t      current_index= 0;
        size_t      token_begin_index = 0;
        size_t      lines_read = 0;
        bool        in_quotes = false;
        for (;;) {
            x_ReadOneLine(is, data, lines_read > 0);
            ++lines_read;

            while (current_index < data->size()) {
                auto    current_char = (*data)[current_index];
                if (current_char == ',') {
                    if (!in_quotes) {
                        m_Tokens.emplace_back(token_begin_index);
                        token_begin_index = current_index + 1;
                    }
                } else if (current_char == '"') {
                    if (!in_quotes && token_begin_index != current_index) {
                        NCBI_THROW(CCRowReaderStream_IANA_CSV_Exception,
                                   eUnexpectedDoubleQuote,
                                   "Unexpected double quote. "
                                   "If a field is not quoted then a "
                                   "double quote may not appear "
                                   "in the middle.");
                    }
                    if (!in_quotes) {
                        in_quotes = true;
                    } else {
                        if (current_index + 1 < data->size() &&
                            (*data)[current_index + 1] == '"') {
                            ++current_index;
                        } else {
                            if (current_index + 1 < data->size() &&
                                (*data)[current_index + 1] != ',')
                                NCBI_THROW(CCRowReaderStream_IANA_CSV_Exception,
                                           eUnexpectedDoubleQuote,
                                           "Unexpected double quote. "
                                           "Closing double quote must be the "
                                           "last in a line or be followed "
                                           "by a comma character");

                            in_quotes = false;
                        }
                    }
                }

                ++current_index;
            }

            if (!in_quotes)
                break;

            // Here: need to read one more line because of the quotes.
            //       So check if we still can read.
            if (!bool(is)) {
                NCBI_THROW(CCRowReaderStream_IANA_CSV_Exception,
                           eUnbalancedDoubleQuote,
                           "Unbalanced double quote detected");
            }
        }

        m_Tokens.push_back(token_begin_index);
        return lines_read;
    }

    ERR_Action OnNextLine(CTempString raw_line)
    {
        if (raw_line.empty())
            return eRR_Skip;
        return eRR_Continue_Data;
    }


    // The tokenization is actually done in the ReadRowData() member
    ERR_Action Tokenize(const CTempString  raw_line,
                        vector<CTempString>& tokens)
    {
        if (m_HasHeader) {
            if (GetMyStream().GetCurrentLineNo() == 0) {
                x_SetFieldNames(raw_line);
                return eRR_Skip;
            }
        }

        size_t      field_size;
        for (TFieldNo field_no = 0; field_no < m_Tokens.size(); ++field_no) {
            if (field_no + 1 < m_Tokens.size())
                field_size = m_Tokens[field_no + 1] - m_Tokens[field_no] - 1;
            else
                field_size = raw_line.size() - m_Tokens[field_no];
            tokens.emplace_back(raw_line.data() + m_Tokens[field_no],
                                field_size);
        }
        return eRR_Continue_Data;
    }

    ERR_Action Validate(CTempString raw_line,
                        ERR_FieldValidationMode field_validation_mode)
    {
        if (field_validation_mode == eRR_NoFieldValidation)
            return eRR_Skip;
        if (m_FieldsToValidate.empty())
            return eRR_Skip;

        if (raw_line.empty())
            return eRR_Skip;

        // Here: the field values need to be validated and there is some type
        // information
        m_ValidationTokens.clear();
        ERR_Action action = this->Tokenize(raw_line, m_ValidationTokens);

        if (action == eRR_Skip)
            return eRR_Skip;

        for (const auto& info : m_FieldsToValidate) {
            if (info.first < m_Tokens.size()) {
                string translated;
                ERR_TranslationResult translation_result =
                    this->Translate((TFieldNo)info.first, m_ValidationTokens[info.first], translated);
                if (translation_result == eRR_UseOriginal) {
                    CRR_Util::ValidateBasicTypeFieldValue(
                        m_ValidationTokens[info.first],
                        info.second.first, info.second.second);
                } else {
                    CRR_Util::ValidateBasicTypeFieldValue(
                        translated, info.second.first, info.second.second);
                }
            }
        }
        return eRR_Skip;
    }

    ERR_TranslationResult Translate(TFieldNo          /* field_no */,
                                    const CTempString raw_value,
                                    string&           translated_value)
    {
        if (!raw_value.empty()) {
            if (raw_value[0] == '"') {
                translated_value = string(raw_value.data() + 1,
                                          raw_value.size() - 2);
                NStr::ReplaceInPlace(translated_value, "\"\"", "\"");
                return eRR_Translated;
            }
        }
        return eRR_UseOriginal;
    }

    /// Tell if the source has a header. The new value will be taken into
    /// account for the further sources which are read from the beginning.
    /// @param has_header
    ///  flag for the following data sources
    void SetHasHeader(bool  has_header)
    { m_HasHeader = has_header; }

    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode event_mode)
    {
        switch (event) {
            case eRR_Event_SourceBegin:
                GetMyStream().x_ClearTraitsProvidedFieldsInfo();

                if (event_mode == eRR_EventMode_Validating)
                    x_GetFieldTypesToValidate();

                // fall through
            case eRR_Event_SourceEnd:
            case eRR_Event_SourceError:
            default:
                ;
        }
        return eRR_EventAction_Default;
    }

private:
    void x_ReadOneLine(CNcbiIstream& is, string* data, bool joining)
    {
        m_RawLine.clear();
        std::getline(is, m_RawLine);
        m_LineSeparator = "\n";
        if(!m_RawLine.empty()  &&  m_RawLine.back() == '\r') {
            m_RawLine.pop_back();
            m_LineSeparator = "\r\n";
        }

        if (joining)
            data->append(m_PreviousLineSeparator);
        data->append(m_RawLine);

        m_PreviousLineSeparator = m_LineSeparator;
    }

    void x_SetFieldNames(const CTempString&  raw_line)
    {
        string      translated;
        size_t      field_size;
        for (TFieldNo field_no = 0; field_no < m_Tokens.size(); ++field_no) {
            if (field_no + 1 < m_Tokens.size())
                field_size = m_Tokens[field_no + 1] - m_Tokens[field_no] - 1;
            else
                field_size = raw_line.size() - m_Tokens[field_no];

            CTempString raw_field_name(raw_line.data() + m_Tokens[field_no],
                                       field_size);

            if (Translate(field_no, raw_field_name, translated) == eRR_UseOriginal)
                GetMyStream().x_SetFieldName(field_no,
                                             string(raw_field_name.data(),
                                                    raw_field_name.size()));
            else
                GetMyStream().x_SetFieldName(field_no, translated);
        }
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

private:
    bool            m_HasHeader;
    vector<size_t>  m_Tokens;
    string          m_LineSeparator;
    string          m_PreviousLineSeparator;
    string          m_RawLine;

    map<size_t, pair<ERR_FieldType, string>> m_FieldsToValidate;
    vector<CTempString>                      m_ValidationTokens;

    RR_TRAITS_PARENT_STREAM(CRowReaderStream_IANA_CSV);
};



/// IANA CSV traits which by default consider the source as the one without a
/// header (can be switched at runtime using the SetHasHeader() member).
/// It follows the specs at:
/// https://tools.ietf.org/html/rfc4180
class CRowReaderStream_IANA_CSV_no_header : public CRowReaderStream_IANA_CSV
{
public:
    CRowReaderStream_IANA_CSV_no_header()
    { SetHasHeader(false); }

private:
    RR_TRAITS_PARENT_STREAM(CRowReaderStream_IANA_CSV_no_header);
};


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_IANA_CSV__HPP */
