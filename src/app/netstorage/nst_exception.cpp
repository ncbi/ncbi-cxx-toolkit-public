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

#include <ncbi_pch.hpp>

#include "nst_exception.hpp"


USING_NCBI_SCOPE;


const char *  CNetStorageServerException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eInvalidArgument:              return "eInvalidArgument";
    case eMandatoryFieldsMissed:        return "eMandatoryFieldsMissed";
    case eHelloRequired:                return "eHelloRequired";
    case eInvalidMessageType:           return "eInvalidMessageType";
    case eInvalidIncomingMessage:       return "eInvalidIncomingMessage";
    case ePrivileges:                   return "ePrivileges";
    case eInvalidMessageHeader:         return "eInvalidMessageHeader";
    case eShuttingDown:                 return "eShuttingDown";
    case eMessageAfterBye:              return "eMessageAfterBye";
    case eStorageError:                 return "eStorageError";
    case eWriteError:                   return "eWriteError";
    case eReadError:                    return "eReadError";
    case eInternalError:                return "eInternalError";
    case eObjectNotFound:               return "eObjectNotFound";
    case eDatabaseError:                return "eDatabaseError";
    case eInvalidConfig:                return "eInvalidConfig";
    default:                            return CException::GetErrCodeString();
    }
}


unsigned int CNetStorageServerException::ErrCodeToHTTPStatusCode(void) const
{
    switch (GetErrCode()) {
    case eInvalidArgument:              return 400;
    case eMandatoryFieldsMissed:        return 400;
    case eHelloRequired:                return 400;
    case eInvalidMessageType:           return 400;
    case eInvalidIncomingMessage:       return 400;
    case ePrivileges:                   return 400;
    case eInvalidMessageHeader:         return 400;
    case eShuttingDown:                 return 503;
    case eMessageAfterBye:              return 400;

    /* The rest of codes convert to status 500. */
    default:                            return 500;
    }
}

