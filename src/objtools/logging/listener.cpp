
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
 *  Objtools message listener classes - based on ILineErrorListener
 *
 */



#include  <ncbi_pch.hpp>
#include <objtools/logging/message.hpp>
#include <objtools/logging/listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool IObjtoolsListener::SevEnabled(EDiagSev severity) const
{
    return (severity > eDiag_Info);
}


CObjtoolsListener::~CObjtoolsListener() = default;


bool 
CObjtoolsListener::PutMessage(const IObjtoolsMessage& message)
{
    m_Messages.emplace_back(message.Clone());
    return true;
}


void
CObjtoolsListener::PutProgress(
    const string& message,
    const Uint8 num_done,
    const Uint8 num_total)
{
    // NB: Some other classes rely on the message fitting in one line.

    // NB: New attributes or inner elements could be added to the resulting
    //     message at any time, so make no assumptions.

    if( ! m_pProgressOstrm ) {
        // no stream to write to
        return;
    }

    *m_pProgressOstrm << "<message severity=\"INFO\" ";

    if( num_done > 0 ) {
        *m_pProgressOstrm << "num_done=\"" << num_done << "\" ";
    }

    if( num_total > 0 ) {
        *m_pProgressOstrm << "num_total=\"" << num_total << "\" ";
    }

    if( message.empty() ) {
        *m_pProgressOstrm  << " />";
    } else {
        *m_pProgressOstrm  << " >";

        string sXMLEncodedMessage = NStr::XmlEncode(message);

        // some functionality relies on progress messages fitting into 
        // one line, so we escape newlines (just in case) while
        // we write it.
        ITERATE( string, msg_it, sXMLEncodedMessage ) {
            const char ch = *msg_it;
            switch(ch) {
            case '\r':
                *m_pProgressOstrm << "&#xD;";
                break;
            case '\n':
                *m_pProgressOstrm << "&#xA;";
                break;
            default:
                *m_pProgressOstrm << ch;
                break;
            }
        }

        *m_pProgressOstrm << "</message>" << NcbiEndl;
    }

    m_pProgressOstrm->flush();
}


void CObjtoolsListener::SetProgressOstream(CNcbiOstream* pProgressOstream)
{
    m_pProgressOstrm = pProgressOstream;
}


const IObjtoolsMessage&
CObjtoolsListener::GetMessage(size_t index) const
{
    return *m_Messages[index].get();
}


size_t
CObjtoolsListener::Count() const
{
    return m_Messages.size();
}


void
CObjtoolsListener::ClearAll() {
    m_Messages.clear();
}


size_t CObjtoolsListener::LevelCount(EDiagSev severity) const {
    size_t uCount = 0;
    for (const auto& pMessage : m_Messages) {
        if (pMessage->GetSeverity() == severity) {
            ++uCount;
        }
    }
    return uCount; 
}


void CObjtoolsListener::Dump(CNcbiOstream& ostr) const 
{
    if (m_Messages.empty()) {
        ostr << "(( No messages ))" << endl;
        return;
    }
    for (const auto& pMessage : m_Messages) {
        pMessage->Dump(ostr);
    }
}


void CObjtoolsListener::DumpAsXML(CNcbiOstream& ostr) const 
{

    if (m_Messages.empty()) {
        ostr << "(( No messages ))" << endl;
        return;
    }

    for (const auto& pMessage : m_Messages) {
        pMessage->DumpAsXML(ostr);
    }
}


CObjtoolsListener::TConstIterator 
CObjtoolsListener::begin() const {
    return TConstIterator(m_Messages.cbegin());
}


CObjtoolsListener::TConstIterator 
CObjtoolsListener::end() const {
    return TConstIterator(m_Messages.cend());
}


CObjtoolsListenerLevel::CObjtoolsListenerLevel(int accept_level) 
    : m_AcceptLevel(accept_level) {}


CObjtoolsListenerLevel::~CObjtoolsListenerLevel() = default;


bool CObjtoolsListenerLevel::PutMessage(const IObjtoolsMessage& message) 
{
    CObjtoolsListener::PutMessage(message);
    return (static_cast<int>(message.GetSeverity()) <= m_AcceptLevel);
}


CObjtoolsListenerLenient::CObjtoolsListenerLenient() 
    : CObjtoolsListenerLevel(eDiag_Info) {}


CObjtoolsListenerStrict::CObjtoolsListenerStrict()
   : CObjtoolsListenerLevel(eDiagSevMax+1) {}


END_SCOPE(objects)
END_NCBI_SCOPE

