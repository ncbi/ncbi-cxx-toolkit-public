#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_EXCEPTION_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_EXCEPTION_HPP_
/*****************************************************************************
 * $Id$
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
 * Authors: Dmitrii Saprykin
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)

class CIpgStorageException: public CException
{
public:
    enum EErrCode
    {
        eIpgGeneralError = 1,
        eIpgHasherInitError,
        eIpgHasherHashError,
        eIpgHasherFetchError,
        eIpgAssignValueZero,
        eIpgAssignValueConflict,
        eIpgUpdateReportNoIpg,
        eIpgUpdateReportWriteProtected,
        eIpgLogicError,
        eIpgNotImplemented,
    };

    const char* GetErrCodeString() const override
    {
        switch ( GetErrCode() ) {
            case eIpgGeneralError: return "eIpgGeneralError";
            case eIpgHasherInitError: return "eIpgHasherInitError";
            case eIpgHasherHashError: return "eIpgHasherHashError";
            case eIpgHasherFetchError: return "eIpgHasherFetchError";
            case eIpgAssignValueZero: return "eIpgAssignValueZero";
            case eIpgAssignValueConflict: return "eIpgAssignValueConflict";
            case eIpgUpdateReportNoIpg: return "eIpgUpdateReportNoIpg";
            case eIpgUpdateReportWriteProtected: return "eIpgUpdateReportWriteProtected";
            case eIpgNotImplemented: return "eIpgNotImplemented";
            case eIpgLogicError: return "eIpgLogicError";
            default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CIpgStorageException, CException);
};

END_SCOPE(ipg)
END_NCBI_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_EXCEPTION_HPP_
