#ifndef DBAPI___DBEXCEPTION__HPP
#define DBAPI___DBEXCEPTION__HPP

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
 * Author:  Michael Kholodov, Denis Vakatov
 *
 * File Description:  DBAPI exception class
 *
 */

#include <dbapi/driver/exception.hpp>


BEGIN_NCBI_SCOPE


class CDbapiException : public CDB_ClientEx
{
public:
    CDbapiException(const CDiagCompileInfo& info,
                    const CException* prev_exception,
                    const string& message)
        : CDB_ClientEx(info,
                       prev_exception,
                       message,
                       eDiag_Error,
                       1000)
    NCBI_DATABASE_EXCEPTION_DEFAULT_IMPLEMENTATION(CDbapiException, CDB_ClientEx);
};

#define NCBI_DBAPI_THROW( message ) \
    throw CDbapiException( DIAG_COMPILE_INFO, 0, (message) )
    
#define CHECK_NCBI_DBAPI( failed, message ) \
    if ( ( failed ) ) { NCBI_DBAPI_THROW( message ); }

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.2  2003/11/04 22:27:14  vakatov
 * CDbapiException to inherit from CDB_ClientEx.
 * Minor style fixes.
 *
 * Revision 1.1  2002/01/30 14:51:23  kholodov
 * User DBAPI implementation, first commit
 * ===========================================================================
 */


#endif  /* DBAPI___DBEXCEPTION__HPP */
