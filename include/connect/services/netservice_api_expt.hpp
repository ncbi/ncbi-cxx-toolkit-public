#ifndef CONNECT_SERVICES___NETSERVICE_API_EXPT__HPP
#define CONNECT_SERVICES___NETSERVICE_API_EXPT__HPP

/*  $Id $
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <connect/services/srv_connections_expt.hpp>

#include <connect/ncbi_core.h>

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE


/// Net Service exception
///
class CNetServiceException : public CException
{
public:
    enum EErrCode {
        eTimeout,
        eCommunicationError,
        eProtocolError,
        eCommandIsNotAllowed
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eTimeout:
            return "eTimeout";
        case eCommunicationError:
            return "eCommunicationError";
        case eProtocolError:
            return "eProtocolError";
        case eCommandIsNotAllowed:
            return "eCommandIsNotAllowed";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetServiceException, CException);
};


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_EXPT__HPP */
