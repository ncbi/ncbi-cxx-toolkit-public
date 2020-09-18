/*
 *
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
 * -----------------
 */

#include <ncbi_pch.hpp>
#include <objtools/flatfile/flatfile_message.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

CFlatFileMessage::CFlatFileMessage(
        const string& module,
        EDiagSev severity,
        int code,
        int subcode, 
        const string& text,
        int lineNum)
    : CObjtoolsMessage(text, severity),
      m_Module(module),
      m_Code(code),
      m_Subcode(subcode),
      m_LineNum(lineNum)
{
}


CFlatFileMessage::~CFlatFileMessage(){}


CFlatFileMessage* CFlatFileMessage::Clone() const
{
    return new CFlatFileMessage(
            m_Module,
            m_Severity,
            m_Code,
            m_Subcode,
            m_Text,
            m_LineNum);
}


void CFlatFileMessage::Write(CNcbiOstream& out) const {
    Dump(out);
}


void CFlatFileMessage::Dump(CNcbiOstream& out) const {

    out << CNcbiDiag::SeverityName(GetSeverity()) << ": ";
    if (!m_Module.empty()) {
        out << m_Module << " ";
    }
    out << GetText() << "\n";
}


void CFlatFileMessage::WriteAsXML(CNcbiOstream& out) const {
    DumpAsXML(out);
}


void CFlatFileMessage::DumpAsXML(CNcbiOstream& out) const {}


const string& CFlatFileMessage::GetModule() const {
    return m_Module;
}


int CFlatFileMessage::GetCode() const { 
    return m_Code;
}


int CFlatFileMessage::GetSubCode() const {
    return m_Subcode;
}


int CFlatFileMessage::GetLineNum() const {
    return m_LineNum;
}


END_SCOPE(objects);
END_NCBI_SCOPE
