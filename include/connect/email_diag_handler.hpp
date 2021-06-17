#ifndef CONNECT___EMAIL_DIAG_HANDLER__HPP
#define CONNECT___EMAIL_DIAG_HANDLER__HPP

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
 * Author:  Aaron M. Ucko <ucko@ncbi.nlm.nih.gov>
 *
 * File Description:
 *   Diagnostic handler for e-mailing logs.
 *
 */

#include <connect/connect_export.h>
#include <corelib/ncbistd.hpp>


/** @addtogroup EmailDiag
 *
 * @{
 */


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CEmailDiagHandler : public CStreamDiagHandler
{
public:
    CEmailDiagHandler(const string& to,
                      const string& subject = "NCBI diagnostics")
        : CStreamDiagHandler(new CNcbiOstrstream, false),
          m_To(to), m_Sub(subject)
        {}
    virtual ~CEmailDiagHandler();

private:
    string m_To;
    string m_Sub;
};


class NCBI_XCONNECT_EXPORT CEmailDiagFactory : public CDiagFactory
{
public:
    virtual CDiagHandler* New(const string& s)
        { return new CEmailDiagHandler(s); }
};


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___EMAIL_DIAG_HANDLER__HPP */
