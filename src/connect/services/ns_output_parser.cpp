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
