#ifndef NETSTORAGE_GC_EXCEPTION__HPP
#define NETSTORAGE_GC_EXCEPTION__HPP

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
 * Authors:  Sergey Satskiy
 *
 */


#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE

const string    k_StopGC = "Stop garbage collecting due to error";
const string    k_UnknownException = "Unknown exception";


class CNetStorageGCException : public CException
{
    public:
        enum EErrCode {
            eDBStructureVersionNotFound     = 1,
            eTooManyDBStructureVersions     = 2,
            eNetStorageServiceNotFound      = 3,
            eExpirationExpected             = 4,
            eStopGC                         = 5
        };
        virtual const char *  GetErrCodeString(void) const;
        NCBI_EXCEPTION_DEFAULT(CNetStorageGCException, CException);
};


END_NCBI_SCOPE


#endif /* NETSTORAGE_GC_EXCEPTION__HPP */

