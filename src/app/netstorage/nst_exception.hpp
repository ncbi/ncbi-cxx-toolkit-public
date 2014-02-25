#ifndef NETSTORAGE_EXCEPTION__HPP
#define NETSTORAGE_EXCEPTION__HPP

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
 * Authors:  Denis Vakatov
 *
 * File Description: Network Storage middleman server exception
 *
 */


#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE


class CNetStorageServerException : public CException
{
    public:
        enum EErrCode {
            eInvalidArgument        = 1001,
            eMandatoryFieldsMissed  = 1002,
            eHelloRequired          = 1003,
            eInvalidMessageType     = 1004,
            eInvalidIncomingMessage = 1005,
            ePrivileges             = 1006,
            eInvalidMessageHeader   = 1007,
            eShuttingDown           = 1008,
            eMessageAfterBye        = 1009,
            eStorageError           = 1010,
            eWriteError             = 1011,
            eReadError              = 1012,
            eInternalError          = 1013,
            eObjectNotFound         = 1014,
            eDatabaseError          = 1015,
            eInvalidConfig          = 1016
        };
        virtual const char *  GetErrCodeString(void) const;
        unsigned int ErrCodeToHTTPStatusCode(void) const;
        NCBI_EXCEPTION_DEFAULT(CNetStorageServerException, CException);
};


END_NCBI_SCOPE


#endif /* NETSTORAGE_EXCEPTION__HPP */

