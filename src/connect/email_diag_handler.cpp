/* $Id$
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
 *   Diagnostic handler for e-mailing logs.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/email_diag_handler.hpp>
#include <connect/ncbi_sendmail.h>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


CEmailDiagHandler::~CEmailDiagHandler(void)
{
    CNcbiOstrstream* oss = dynamic_cast<CNcbiOstrstream*>(m_Stream);
    string body = CNcbiOstrstreamToString(*oss);
    if ( !body.empty() ) {
        const char* msg = CORE_SendMail(m_To.c_str(), m_Sub.c_str(), body.c_str());
        if (msg) {
            // Need to report by other means, obviously...
            NcbiCerr << msg << endl;
        }
    }
    delete m_Stream;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
