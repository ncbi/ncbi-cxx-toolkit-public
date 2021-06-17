#ifndef UTIL___ROW_READER_EXCEL_CSV__HPP
#define UTIL___ROW_READER_EXCEL_CSV__HPP

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
*   Implementation of the CRowReader<> traits for MS EXCEL CSV
*
* ===========================================================================
*/

#include <util/row_reader_char_delimited.hpp>


BEGIN_NCBI_SCOPE


/// Note 1: Empty rows are allowed and treated as 0 fields rows
/// Note 2: Both CRLF and LF are allowed
/// Note 3: Number of fields is not enforced
/// Note 4: There is no formal MS Excel CSV spec. So the implementation is
///         based on experiments made on MS Excel 2013.
///         See the description in JIRA: CXX-9221
/// Note 5: Two field cases in a data source
///         - empty, i.e. ,,
///         - "", i.e. ,"",
///         are translated to a Null field
/// Note 6: trailing Null fields in a data source are stripped


const CTempString   kNullFieldRepresentation = CTempString("\"\"", 2);


/// MS Excel CSV traits.
class CRowReaderStream_Excel_CSV : public TRowReaderStream_SingleCommaDelimited
{
public:
    CRowReaderStream_Excel_CSV()
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
                    if (token_begin_index == current_index) {
                        in_quotes = true;
                    } else {
                        if (in_quotes) {
                            if (current_index + 1 < data->size() &&
                                (*data)[current_index + 1] == '"') {
                                ++current_index;
                            } else {
                                in_quotes = false;
                            }
                        }
                    }
                }

                ++current_index;
            }

            if (!in_quotes)
                break;

            // Here: need to read one more line because of the double quotes.
            //       So check if we still can read.
            if (!bool(is))
                break;
        }

        m_Tokens.push_back(token_begin_index);
        return lines_read;
    }

    ERR_Action OnNextLine(CTempString /* raw_line */)
    {
        return eRR_Continue_Data;
    }


    // The tokenization is actually done in the ReadRowData() member
    ERR_Action Tokenize(const CTempString  raw_line,
                        vector<CTempString>& tokens)
    {
        // Special case in accordance with CXX-9221: empty line => no fields
        if (!raw_line.empty()) {
            size_t      field_size;
            for (TFieldNo field_no = 0;
                 field_no < m_Tokens.size(); ++field_no) {
                if (field_no + 1 < m_Tokens.size())
                    field_size = m_Tokens[field_no + 1] - m_Tokens[field_no] - 1;
                else
                    field_size = raw_line.size() - m_Tokens[field_no];
                tokens.emplace_back(raw_line.data() + m_Tokens[field_no],
                                    field_size);
            }

            x_StripTrailingNullFields(tokens);
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
        if (x_IsNull(raw_value))
            return eRR_Null;

        if (raw_value[0] == '=') {
            size_t  dbl_quote_cnt = 0;
            for (size_t index = 0; index < raw_value.size(); ++index)
                if (raw_value[index] == '"')
                    ++dbl_quote_cnt;

            if (dbl_quote_cnt == 0) {
                translated_value = string(raw_value.data() + 1,
                                          raw_value.size() - 1);
                return eRR_Translated;
            }

            // Here: there are " in the field. They may need to be stripped
            // together with = if:
            // - " follows = immediately
            // - " is the last character in a field
            // - there is an even number of "
            // If so then "" need to be translated into " inside the field
            // as well
            if (dbl_quote_cnt % 2 == 0) {
                if (raw_value[1] == '"' &&
                    raw_value[raw_value.size() - 1] == '"') {
                    // Balanced double quote and poperly surround the field
                    // value => strip the = and surrounding " plus replace
                    // "" with "
                    translated_value = string(raw_value.data() + 2,
                                              raw_value.size() - 3);
                    NStr::ReplaceInPlace(translated_value, "\"\"", "\"");
                    return eRR_Translated;
                }
            }

            // Non balanced double quotes or they are not surrounding the
            // value after =
            // There is no translation for this case
            return eRR_UseOriginal;
        }

        if (raw_value[0] == '"') {
            size_t      match_index = 1;
            for (; match_index < raw_value.size(); ++match_index) {
                if (raw_value[match_index] == '"') {
                    if (match_index + 1< raw_value.size() &&
                        raw_value[match_index + 1] == '"')
                        ++match_index;
                    else
                        break;
                }
            }

            // Here: match_index points beyond of the field or to a
            // matching "
            if (match_index < raw_value.size()) {
                // matching " found
                translated_value = string(raw_value.data() + 1,
                                          match_index - 1);
                NStr::ReplaceInPlace(translated_value, "\"\"", "\"");
                if (match_index < raw_value.size() - 1) {
                    // tail of the field needs to ba attached as is
                    translated_value.append(
                        raw_value.data() + match_index + 1,
                        raw_value.size() - match_index - 1);
                }
            } else {
                // Unbalanced " case
                translated_value = string(raw_value.data() + 1,
                                          raw_value.size() - 1);
            }

            // This could be a case with a leading = which may need to be
            // stripped as well...
            if (!translated_value.empty()) {
                if (translated_value[0] == '=') {
                    size_t  dbl_quote_cnt = 0;
                    for (size_t index = 0;
                         index < translated_value.size(); ++index)
                        if (translated_value[index] == '"')
                            ++dbl_quote_cnt;

                    if (dbl_quote_cnt > 0 && (dbl_quote_cnt % 2 == 0)) {
                        if (translated_value[1] == '"' &&
                            translated_value[translated_value.size() - 1] == '"') {
                            translated_value = translated_value.substr(2, translated_value.size() - 3);
                        }
                    }
                }
            }

            return eRR_Translated;
        }
        return eRR_UseOriginal;
    }

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

    bool x_IsNull(const CTempString&  raw_field_value)
    {
        return raw_field_value.empty() ||
               (raw_field_value == kNullFieldRepresentation);
    }

    void x_StripTrailingNullFields(vector<CTempString>& tokens)
    {
        while (!tokens.empty()) {
            if (x_IsNull(tokens.back()))
                tokens.pop_back();
            else
                break;
        }
    }

private:
    vector<size_t>  m_Tokens;
    string          m_LineSeparator;
    string          m_PreviousLineSeparator;
    string          m_RawLine;

    map<size_t, pair<ERR_FieldType, string>> m_FieldsToValidate;
    vector<CTempString>                      m_ValidationTokens;

    RR_TRAITS_PARENT_STREAM(CRowReaderStream_Excel_CSV);
};



END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_EXCEL_CSV__HPP */
