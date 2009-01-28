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
 * File Description:
 *   Contains declarations of utility classes used by Grid applications.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */


#ifndef CONNECT_SERVICES___UTIL__HPP
#define CONNECT_SERVICES___UTIL__HPP

#include "netobject.hpp"

BEGIN_NCBI_SCOPE

struct SCmdLineArgListImpl;

class NCBI_XCONNECT_EXPORT CCmdLineArgList
{
    NET_COMPONENT(CmdLineArgList);

    static CCmdLineArgList OpenForOutput(const std::string& file_or_stdout);

    void WriteLine(const std::string& line);

    static CCmdLineArgList CreateFrom(const std::string& file_or_list);

    bool GetNextArg(std::string& arg);

    static const std::string& GetDelimiterString();
};

class NCBI_XCONNECT_EXPORT CBitVectorEncoder
{
public:
    CBitVectorEncoder() : m_RangeFrom(1), m_RangeTo(0) {}

    void AppendInteger(unsigned number);
    void AppendRange(unsigned from, unsigned to);

    std::string Encode();

private:
    void FlushRange();

    unsigned m_RangeFrom;
    unsigned m_RangeTo;
    std::string m_String;
    std::string m_Buffer;
};

inline std::string CBitVectorEncoder::Encode()
{
    if (m_RangeFrom <= m_RangeTo)
        FlushRange();

    return m_String;
}

class NCBI_XCONNECT_EXPORT CBitVectorDecoder
{
public:
    CBitVectorDecoder(const std::string source);

    bool GetNextRange(unsigned& from, unsigned& to);

private:
    std::string m_Source;
    const char* m_Ptr;
};

inline CBitVectorDecoder::CBitVectorDecoder(const std::string source) :
    m_Source(source), m_Ptr(m_Source.c_str())
{
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___UTIL__HPP */
