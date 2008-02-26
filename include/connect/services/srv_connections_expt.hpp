#ifndef CONNECT_SERVICES__SRV_CONNECTIONS_EXEPT_HPP
#define CONNECT_SERVICES__SRV_CONNECTIONS_EXEPT_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:
 *
 */

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE



/// Net Service exception
///
class CNetSrvConnException : public CException
{
public:
    enum EErrCode {
        eReadTimeout,
        eResponseTimeout,
        eLBNameNotFound,
        eSrvListEmpty,
        eConnectionFailure,
        eWriteFailure
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eReadTimeout:        return "eReadTimeout";
        case eResponseTimeout:    return "eResponseTimeout";
        case eLBNameNotFound:     return "eLBNameNotFound";
        case eSrvListEmpty:       return "eSrvListEmpty";
        case eConnectionFailure:  return "eConntectionFailure";
        case eWriteFailure:       return "eWriteFailure";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetSrvConnException, CException);
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SRV_CONNECTIONS_EXEPT_HPP
