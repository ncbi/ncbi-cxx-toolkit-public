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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: NetSchedule output parsers - implementation.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/ns_output_parser.hpp>


BEGIN_NCBI_SCOPE

#define INVALID_FORMAT_ERROR() \
    NCBI_THROW2(CStringException, eFormat, \
            (*m_Ch == '\0' ? "Unexpected end of NetSchedule output" : \
                    "Syntax error in structured NetSchedule output"), \
            GetPosition())

CJsonNode CNetScheduleStructuredOutputParser::ParseJSON(const string& json)
{
    m_Ch = (m_NSOutput = json).c_str();

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    CJsonNode root;

    switch (*m_Ch) {
    case '[':
        ++m_Ch;
        root = ParseArray(']');
        break;

    case '{':
        ++m_Ch;
        root = ParseObject('}');
        break;

    default:
        INVALID_FORMAT_ERROR();
    }

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch != '\0') {
        INVALID_FORMAT_ERROR();
    }

    return root;
}

string CNetScheduleStructuredOutputParser::ParseString(size_t max_len)
{
    size_t len;
    string val(NStr::ParseQuoted(CTempString(m_Ch, max_len), &len));

    m_Ch += len;
    return val;
}

Int8 CNetScheduleStructuredOutputParser::ParseInt(size_t len)
{
    Int8 val = NStr::StringToInt8(CTempString(m_Ch, len));

    if (*m_Ch == '-') {
        ++m_Ch;
        --len;
    }
    if (*m_Ch == '0' && len > 1) {
        NCBI_THROW2(CStringException, eFormat,
                "Leading zeros are not allowed", GetPosition());
    }

    m_Ch += len;
    return val;
}

double CNetScheduleStructuredOutputParser::ParseDouble(size_t len)
{
    double val = NStr::StringToDouble(CTempString(m_Ch, len));

    m_Ch += len;
    return val;
}

bool CNetScheduleStructuredOutputParser::MoreNodes()
{
    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;
    if (*m_Ch != ',')
        return false;
    while (isspace((unsigned char) *++m_Ch))
        ;
    return true;
}

CJsonNode CNetScheduleStructuredOutputParser::ParseObject(char closing_char)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    while (*m_Ch == '\'' || *m_Ch == '"') {
        // New attribute/value pair
        string attr_name(ParseString(GetRemainder()));

        while (isspace((unsigned char) *m_Ch))
            ++m_Ch;
        if (*m_Ch == ':' || *m_Ch == '=')
            while (isspace((unsigned char) *++m_Ch))
                ;

        result.SetByKey(attr_name, ParseValue());

        if (!MoreNodes()) {
            if (*m_Ch != closing_char)
                break;
            ++m_Ch;
            return result;
        }
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CNetScheduleStructuredOutputParser::ParseArray(char closing_char)
{
    CJsonNode result(CJsonNode::NewArrayNode());

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    do
        result.Append(ParseValue());
    while (MoreNodes());

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CNetScheduleStructuredOutputParser::ParseValue()
{
    size_t max_len = GetRemainder();
    size_t len = 0;

    switch (*m_Ch) {
    /* Array */
    case '[':
        ++m_Ch;
        return ParseArray(']');

    /* Object */
    case '{':
        ++m_Ch;
        return ParseObject('}');

    /* String */
    case '\'':
    case '"':
        return CJsonNode::NewStringNode(ParseString(max_len));

    /* Number */
    case '-':
        // Check that there's at least one digit after the minus sign.
        if (max_len <= 1 || !isdigit((unsigned char) m_Ch[1])) {
            ++m_Ch;
            break;
        }
        len = 1;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        // Skim through the integer part.
        do
            if (++len >= max_len)
                return CJsonNode::NewIntegerNode(ParseInt(len));
        while (isdigit((unsigned char) m_Ch[len]));

        // Stumbled upon a non-digit character -- check
        // if it's a fraction part or an exponent part.
        switch (m_Ch[len]) {
        case '.':
            if (++len == max_len || !isdigit((unsigned char) m_Ch[len])) {
                NCBI_THROW2(CStringException, eFormat,
                        "At least one digit after the decimal "
                        "point is required", GetPosition());
            }
            for (;;) {
                if (++len == max_len)
                    return CJsonNode::NewDoubleNode(ParseDouble(len));

                if (!isdigit((unsigned char) m_Ch[len])) {
                    if (m_Ch[len] == 'E' || m_Ch[len] == 'e')
                        break;

                    return CJsonNode::NewDoubleNode(ParseDouble(len));
                }
            }
            /* FALL THROUGH */

        case 'E':
        case 'e':
            if (++len == max_len ||
                    (m_Ch[len] == '-' || m_Ch[len] == '+' ?
                            ++len == max_len ||
                                    !isdigit((unsigned char) m_Ch[len]) :
                            !isdigit((unsigned char) m_Ch[len]))) {
                m_Ch += len;
                NCBI_THROW2(CStringException, eFormat,
                        "Invalid exponent specification", GetPosition());
            }
            while (++len < max_len && isdigit((unsigned char) m_Ch[len]))
                ;
            return CJsonNode::NewDoubleNode(ParseDouble(len));

        default:
            return CJsonNode::NewIntegerNode(ParseInt(len));
        }

    /* Constant */
    case 'F': case 'f': case 'N': case 'n':
    case 'T': case 't': case 'Y': case 'y':
        while (len <= max_len && isalpha((unsigned char) m_Ch[len]))
            ++len;

        {
            CTempString val(m_Ch, len);
            m_Ch += len;
            return val == "null" ? CJsonNode::NewNullNode() :
                CJsonNode::NewBooleanNode(NStr::StringToBool(val));
        }
    }

    INVALID_FORMAT_ERROR();
}

CAttrListParser::ENextAttributeType CAttrListParser::NextAttribute(
    CTempString* attr_name, string* attr_value, size_t* attr_column)
{
    while (isspace(*m_Position))
        ++m_Position;

    if (*m_Position == '\0')
        return eNoMoreAttributes;

    const char* attr_start = m_Position;
    *attr_column = attr_start - m_Start + 1;

    for (;;)
        if (*m_Position == '=') {
            attr_name->assign(attr_start, m_Position - attr_start);
            break;
        } else if (isspace(*m_Position)) {
            attr_name->assign(attr_start, m_Position - attr_start);
            while (isspace(*++m_Position))
                ;
            if (*m_Position == '=')
                break;
            else
                return eStandAloneAttribute;
        } else if (*++m_Position == '\0') {
            attr_name->assign(attr_start, m_Position - attr_start);
            return eStandAloneAttribute;
        }

    // Skip the equals sign and the spaces that may follow it.
    while (isspace(*++m_Position))
        ;

    attr_start = m_Position;

    switch (*m_Position) {
    case '\0':
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "empty attribute value must be specified as " <<
                attr_name << "=\"\"");
    case '\'':
    case '"':
        {
            size_t n_read;
            *attr_value = NStr::ParseQuoted(CTempString(attr_start,
                m_EOL - attr_start), &n_read);
            m_Position += n_read;
        }
        break;
    default:
        while (*++m_Position != '\0' && !isspace(*m_Position))
            ;
        *attr_value = NStr::ParseEscapes(
            CTempString(attr_start, m_Position - attr_start));
    }

    return eAttributeWithValue;
}

END_NCBI_SCOPE
