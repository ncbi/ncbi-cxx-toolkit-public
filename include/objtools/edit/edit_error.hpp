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
 *  objtools/edit message class
 *
 */

#ifndef _OBJTOOLS_EDIT_ERROR_HPP_
#define _OBJTOOLS_EDIT_ERROR_HPP_

#include <objtools/logging/message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJUTIL_EXPORT CObjEditMessage : public IObjtoolsMessage
//  ============================================================================
{
public:
    CObjEditMessage(const string& text,
                    EDiagSev severity)
        : m_Text(text), m_Severity(severity) {}


    virtual CObjEditMessage *Clone(void) const {
        return new CObjEditMessage(m_Text, m_Severity);
    }

    virtual string
    Compose(void) const
    {
        return x_GetSeverityString() + ": " + GetText();
    }

    virtual void Write(
        CNcbiOstream& out ) const
    {
        out << "                " <<
           x_GetSeverityString()
           << ":" << endl;
        out << "Problem:        " <<
            GetText() << endl;
        out << endl;
    };

    virtual void Dump(
        CNcbiOstream& out) const
    {
        Write(out);
    };

    // dump the XML on one line since some tools assume that
    virtual void WriteAsXML(
        CNcbiOstream& out ) const
    {
        out << "<message severity=\"" << NStr::XmlEncode(x_GetSeverityString()) << "\" "
            << "problem=\"" << NStr::XmlEncode(GetText()) << "\" ";
        out << "</message>" << endl;
    };

    virtual void DumpAsXML(
        CNcbiOstream& out) const
    {
        WriteAsXML(out);
    };

    virtual string GetText(void) const { return m_Text; }
    virtual EDiagSev GetSeverity(void) const { return m_Severity; }
    virtual int GetCode(void) const { return 0; }
    virtual int GetSubCode(void) const { return 0; }

private:

    string x_GetSeverityString(void) const
    {
        return CNcbiDiag::SeverityName(GetSeverity());
    }
    string m_Text;
    EDiagSev m_Severity;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _OBJTOOLS_EDIT_ERROR_HPP_
