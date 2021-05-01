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
 * Author: Justin Foley
 *
 * File Description:
 *  Basic objtools message interface
 *
 */

#ifndef _READER_MESSAGE_HPP_
#define _READER_MESSAGE_HPP_

#include <objtools/logging/message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CReaderMessage:
    public CObjtoolsMessage
//  ============================================================================
{
public:
    CReaderMessage(
        EDiagSev severity, int lineNumber, const string& message)
        :
        CObjtoolsMessage(message, severity), m_LineNumber(lineNumber)
    {};

    virtual CReaderMessage *Clone() const override;

    virtual void Write(CNcbiOstream& out) const override;

    virtual string Message() const { return GetText(); };
    virtual EDiagSev Severity() const { return GetSeverity(); };
    virtual int LineNumber() const { return m_LineNumber; };

    void SetLineNumber(int lineNumber) {
        if (m_LineNumber == 0) m_LineNumber = lineNumber;
    };

protected:
    int m_LineNumber;
};

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CReaderProgress:
    public CProgressMessage
//  ============================================================================
{
public:
    CReaderProgress(int done, int total):
        CProgressMessage(done, total) {};

    virtual void Write(CNcbiOstream& out) const override;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _READER_MESSAGE_HPP_
