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
 * File Description: NetSchedule output parsers - declarations.
 *
 */

#ifndef CONNECT__SERVICES__NS_OUTPUT_PARSER__HPP
#define CONNECT__SERVICES__NS_OUTPUT_PARSER__HPP

#include "netschedule_api.hpp"

BEGIN_NCBI_SCOPE

class NCBI_XCONNECT_EXPORT CAttrListParser
{
public:
    enum ENextAttributeType {
        eAttributeWithValue,
        eStandAloneAttribute,
        eNoMoreAttributes
    };

    void Reset(const char* position, const char* eol)
    {
        m_Position = m_Start = position;
        m_EOL = eol;
    }

    void Reset(const CTempString& line)
    {
        const char* line_buf = line.data();
        Reset(line_buf, line_buf + line.size());
    }

    ENextAttributeType NextAttribute(CTempString* attr_name,
            string* attr_value, size_t* attr_column);

private:
    const char* m_Start;
    const char* m_Position;
    const char* m_EOL;
};

END_NCBI_SCOPE

#endif // CONNECT__SERVICES__NS_OUTPUT_PARSER__HPP
