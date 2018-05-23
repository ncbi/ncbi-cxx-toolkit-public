/*
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
 * Author:  Michael Kornbluh
 *
 * File Description:
 *   Classes for listening to errors, progress, etc.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

void
CMessageListenerBase::PutProgress(
    const string & sMessage,
    const Uint8 iNumDone,
    const Uint8 iNumTotal)
{
    // NB: Some other classes rely on the message fitting in one line.

    // NB: New attributes or inner elements could be added to the resulting
    //     message at any time, so make no assumptions.

    if( ! m_pProgressOstrm ) {
        // no stream to write to
        return;
    }

    *m_pProgressOstrm << "<message severity=\"INFO\" ";

    if( iNumDone > 0 ) {
        *m_pProgressOstrm << "num_done=\"" << iNumDone << "\" ";
    }

    if( iNumTotal > 0 ) {
        *m_pProgressOstrm << "num_total=\"" << iNumTotal << "\" ";
    }

    if( sMessage.empty() ) {
        *m_pProgressOstrm  << " />";
    } else {
        *m_pProgressOstrm  << " >";

        string sXMLEncodedMessage = NStr::XmlEncode(sMessage);

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

END_SCOPE(objects)
END_NCBI_SCOPE

