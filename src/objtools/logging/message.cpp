
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
 *  Error-reporting/message classes for use in objtools
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/logging/message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static string s_GetSeverityName(EDiagSev severity) 
{
    return CNcbiDiag::SeverityName(severity);
}

CObjtoolsMessage::CObjtoolsMessage(const string& text, EDiagSev severity)
: m_Text(text), m_Severity(severity) {}


CObjtoolsMessage* CObjtoolsMessage::Clone(void) const 
{
    return new CObjtoolsMessage(m_Text, m_Severity);
}


string CObjtoolsMessage::Compose(void) const
{
    return s_GetSeverityName(GetSeverity()) + ": " + GetText();
}


void CObjtoolsMessage::Write(CNcbiOstream& out) const
{
    out << "                " <<  
        s_GetSeverityName(GetSeverity())
        << ":\n"; 
    out << "Problem:        " <<  
        GetText() << "\n";
    out << endl;
}


void CObjtoolsMessage::Dump(CNcbiOstream& out) const
{
    Write(out);
}


void CObjtoolsMessage::WriteAsXML(CNcbiOstream& out) const 
{
    out << "<message severity=\"" << NStr::XmlEncode(s_GetSeverityName(GetSeverity())) << "\" "
        << "problem=\"" << NStr::XmlEncode(GetText()) << "\" ";
    out << "</message>" << endl;
}

void CObjtoolsMessage::DumpAsXML(CNcbiOstream& out) const 
{
    WriteAsXML(out);
}


string CObjtoolsMessage::GetText(void) const 
{ 
    return m_Text;
}


EDiagSev CObjtoolsMessage::GetSeverity(void) const 
{
    return m_Severity;
}


int CObjtoolsMessage::GetCode(void) const
{
    return 0;
}


int CObjtoolsMessage::GetSubCode(void) const
{
    return 0;
}


CProgressMessage::CProgressMessage(
    int done,
    int total): CObjtoolsMessage("", eDiag_Info)
{
    if (total) {
        m_Text = NStr::IntToString(mDone) + " of " + NStr::IntToString(mTotal) + " done.";
    }
    else {
        m_Text = NStr::IntToString(mDone) + " done.";
    }
};

CProgressMessage* CProgressMessage::Clone() const 
{
    return new CProgressMessage(mDone, mTotal);
}

void CProgressMessage::Write(CNcbiOstream& ostr) const
{
    ostr << "                " <<  s_GetSeverityName(GetSeverity()) << ":" << endl;
    ostr << "Progress:       " <<  GetText() << endl;
    ostr << endl;
}


END_SCOPE(objects)
END_NCBI_SCOPE

