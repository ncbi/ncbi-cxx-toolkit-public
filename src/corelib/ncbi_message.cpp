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
 * Authors:  Aleksey Grichenko
 *
 * File Description:   IMessage/IMessageListener implementation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_message.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


CMessage_Base::CMessage_Base(const string& txt,
                             EDiagSev      sev,
                             int           err_code,
                             int           sub_code)
    : m_Text(txt),
      m_Severity(sev),
      m_ErrCode(err_code),
      m_SubCode(sub_code)
{
}


string CMessage_Base::GetText(void) const
{
    return m_Text;
}


EDiagSev CMessage_Base::GetSeverity(void) const
{
    return m_Severity;
}


int CMessage_Base::GetCode(void) const
{
    return m_ErrCode;
}


int CMessage_Base::GetSubCode(void) const
{
    return m_SubCode;
}


IMessage* CMessage_Base::Clone(void) const
{
    return new CMessage_Base(*this);
}


void CMessage_Base::Write(CNcbiOstream& out) const
{
    out << CNcbiDiag::SeverityName(GetSeverity()) << ": " << GetText() << endl;
}


string CMessage_Base::Compose(void) const
{
    CNcbiOstrstream out;
    Write(out);
    return CNcbiOstrstreamToString(out);
}


void CListener_Base::PostMessage(const IMessage& message)
{
    m_Messages.push_back(AutoPtr<IMessage>(message.Clone()));
}


void CListener_Base::PostProgress(const string& message,
                                  Uint8         current,
                                  Uint8         total)
{
    ERR_POST(Note << message << " [" << current << "/" << total << "]");
}


const IMessage& CListener_Base::GetMessage(size_t index) const
{
    return *m_Messages[index].get();
}


size_t CListener_Base::Count(void) const
{
    return m_Messages.size();
}


void CListener_Base::Clear(void)
{
    m_Messages.clear();
}


END_NCBI_SCOPE
