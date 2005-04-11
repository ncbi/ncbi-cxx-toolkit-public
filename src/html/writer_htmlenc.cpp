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
* Author:  Aaron Ucko
*
* File Description:
*   CWriter_HTMLEncoder -- HTML-encode supplied data on the fly before
*   passing it to a standard ostream.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <html/writer_htmlenc.hpp>
#include <html/htmlhelper.hpp>

BEGIN_NCBI_SCOPE


CWriter_HTMLEncoder::~CWriter_HTMLEncoder()
{
    if (m_Flags & fTrailingAmpersand) {
        m_Stream << "&amp;";
    }
}


ERW_Result CWriter_HTMLEncoder::Write(const void* buf, size_t count,
                                      size_t* bytes_written)
{
    // Using HTMLHelper::HTMLEncode might have been somewhat more
    // efficient, but this approach handles corner cases more readily.

    const char* p = static_cast<const char*>(buf);

    if ((m_Flags & fTrailingAmpersand)  &&  count) {
        if (p[0] == '#') {
            m_Stream << '&';
        } else {
            m_Stream << "&amp;";
        }
        m_Flags &= ~fTrailingAmpersand;
    }

    size_t n;
    for (n = 0;  n < count  &&  m_Stream;  ++n) {
        switch (p[n]) {
        case '&':
            if ((m_Flags & fPassNumericEntities)  &&  n == count - 1) {
                m_Flags |= fTrailingAmpersand;
            } else if ((m_Flags & fPassNumericEntities)  &&  p[n + 1] == '#') {
                m_Stream << '&';
            } else {
                m_Stream << "&amp;";
            }
            break;

        case '"':  m_Stream << "&quot;";  break;
        case '<':  m_Stream << "&lt;";    break;
        case '>':  m_Stream << "&gt;";    break;
        default:   m_Stream << p[n];      break;
        }
    }
    if (bytes_written) {
        *bytes_written = n;
    }

    return m_Stream.eof() ? eRW_Eof : m_Stream.bad() ? eRW_Error : eRW_Success;
}


ERW_Result CWriter_HTMLEncoder::Flush(void)
{
    // Ignores m_TrailingAmp, because flushing may not come at a semantic
    // boundary.
    m_Stream.flush();
    return m_Stream.eof() ? eRW_Eof : m_Stream.bad() ? eRW_Error : eRW_Success;
}


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/04/11 17:59:51  ucko
* CWriter_HTMLEncoder -- HTML-encode supplied data on the fly before
* passing it to a standard ostream.  (Initial revision.)
*
*
* ===========================================================================
*/
