#ifndef UTIL___ROW_READER_NCBI_TSV__HPP
#define UTIL___ROW_READER_NCBI_TSV__HPP

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
*   Implementation of the CRowReader<> traits for NCBI TSV
*
* ===========================================================================
*/

#include <util/row_reader_char_delimited.hpp>


BEGIN_NCBI_SCOPE


/// The following rules are going to be applied for the NCBI TSV sources:
/// - each line is processed independently (no line merging at all)
/// - line separators supported: CRLF and LF
/// - field separator is '\t'
/// - lines may have variable number of fields (not matching the number of
///   field names)
/// - empty lines are skipped
/// - lines which start with '##' are considered metadata and passed to the
///   user as rows of type eRR_Metadata
/// - lines which start with '#':
///   - if it is a first non empty line then it is a comment line with field
///     names. The names are extracted and the line is not passed to the user
///   - otherwise the line is treated as a comment, and passed to the user
/// - empty column names are allowed
/// - empty values are allowed
/// - the last line EOL is not enforced
/// - the following translation is done:
///   - '-' is translated to '' (empty string)
///   - 'na' is translated to Null value
/// - validation depends on the mode:
///   - no field validation: only reads the source and does not enforce
///     anything
///   - field validation: reads the source and if the basic type information is
///     supplied by the user, then attempts to convert the raw value to
///     the required typed value. In case of problems an exception is
///     generated.
///     Note: a custom format could be supplied for the eRR_DateTime fields.



/// NCBI TSV traits.
/// It follows the specs outlined above
class CRowReaderStream_NCBI_TSV : public TRowReaderStream_SingleTabDelimited
{
public:
    CRowReaderStream_NCBI_TSV() :
        m_NonEmptyLineFound(false)
    {}

    ERR_Action OnNextLine(CTempString raw_line)
    {
        if (raw_line.empty())
            return eRR_Skip;
        if (NStr::StartsWith(raw_line, "##")) {
            m_NonEmptyLineFound = true;
            return eRR_Continue_Metadata;
        }
        if (NStr::StartsWith(raw_line, "#")) {
            if (m_NonEmptyLineFound)
                return eRR_Continue_Comment;
            x_ExtractNames(raw_line);
            m_NonEmptyLineFound = true;
            return eRR_Skip;
        }
        m_NonEmptyLineFound = true;
        return eRR_Continue_Data;
    }

    ERR_Action Validate(CTempString raw_line,
                        ERR_FieldValidationMode field_validation_mode)
    {
        if (field_validation_mode == eRR_NoFieldValidation)
            return eRR_Skip;
        if (m_FieldsToValidate.empty())
            return eRR_Skip;

        ERR_Action action = this->OnNextLine(raw_line);
        if (action != eRR_Continue_Data)
            return eRR_Skip;

        m_ValidationTokens.clear();
        this->Tokenize(raw_line, m_ValidationTokens);

        for (const auto& info : m_FieldsToValidate) {
            if (info.first < m_ValidationTokens.size()) {
                string translated;
                ERR_TranslationResult translation_result =
                    this->Translate((TFieldNo)info.first, m_ValidationTokens[info.first], translated);
                switch (translation_result) {
                    case eRR_UseOriginal:
                        CRR_Util::ValidateBasicTypeFieldValue(
                            m_ValidationTokens[info.first],
                            info.second.first, info.second.second);
                        break;
                    case eRR_Translated:
                        CRR_Util::ValidateBasicTypeFieldValue(
                            translated, info.second.first, info.second.second);
                        break;
                    case eRR_Null:
                    default:
                        ;
                }
            }
        }
        return eRR_Skip;
    }

    ERR_EventAction OnEvent(ERR_Event event,
                            ERR_EventMode event_mode)
    {
        switch (event) {
            case eRR_Event_SourceBegin:
                GetMyStream().x_ClearTraitsProvidedFieldsInfo();
                m_NonEmptyLineFound = false;

                if (event_mode == eRR_EventMode_Validating)
                    x_GetFieldTypesToValidate();

                return eRR_EventAction_Continue;

            case eRR_Event_SourceEnd:
            case eRR_Event_SourceError:
            default:
                ;
        }
        return eRR_EventAction_Default;
    }

    ERR_TranslationResult Translate(TFieldNo          /* field_no */,
                                    const CTempString raw_value,
                                    string&           translated_value)
    {
        if (raw_value == "-") {
            translated_value = "";
            return eRR_Translated;
        }
        if (raw_value == "na")
            return eRR_Null;
        return eRR_UseOriginal;
    }

private:
    bool                    m_NonEmptyLineFound;

    void x_ExtractNames(const CTempString& raw_line)
    {
        vector<CTempString>     tokens;
        Tokenize(CTempString(raw_line.data() + 1, raw_line.size() - 1),
                 tokens);

        TFieldNo    field_no = 0;
        for (const auto&  name : tokens) {
            GetMyStream().x_SetFieldName(field_no,
                                         string(name.data(), name.length()));
            ++field_no;
        }
    }

private:
    vector<CTempString>                      m_ValidationTokens;
    map<size_t, pair<ERR_FieldType, string>> m_FieldsToValidate;

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

    RR_TRAITS_PARENT_STREAM(CRowReaderStream_NCBI_TSV);
};


END_NCBI_SCOPE

#endif  /* UTIL___ROW_READER_NCBI_TSV__HPP */
