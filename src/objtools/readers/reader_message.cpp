
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
 * Author: Frank Ludwig, derived from work by Justin Foley
 *
 * File Description:
 *  Error-reporting/message classes for use in readers
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/readers/reader_message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static string sGetSeverityName(EDiagSev severity)
{
    return CNcbiDiag::SeverityName(severity);
}

//  ============================================================================
CReaderMessage* CReaderMessage::Clone() const
//  ============================================================================
{
    return new CReaderMessage(GetSeverity(), m_LineNumber, GetText());
}

//  ============================================================================
void CReaderMessage::Write(CNcbiOstream& ostr) const
//  ============================================================================
{
    ostr << "                " <<  sGetSeverityName(Severity()) << endl;
    if (LineNumber()) {
        ostr << "Line:           " << LineNumber() << endl;
    }
    ostr << "Problem:        " <<  Message() << endl;
    ostr << endl;
}

//  ============================================================================
void CReaderProgress::Write(CNcbiOstream& ostr) const
//  ============================================================================
{
    ostr << "                " <<  sGetSeverityName(GetSeverity()) << endl;
    ostr << "Progress:       " <<  GetText() << endl;
    ostr << endl;
}

END_SCOPE(objects)
END_NCBI_SCOPE

