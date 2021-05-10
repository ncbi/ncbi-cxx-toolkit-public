#ifndef PUBSEQ_GATEWAY_EXCEPTION__HPP
#define PUBSEQ_GATEWAY_EXCEPTION__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: exceptions used in the pubseq gateway server
 *
 */

#include "uv.h"

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CPubseqGatewayException : public CException
{
public:
    enum EErrCode {
        eRequestAlreadyStarted,
        eRequestCancelled,
        eRequestGeneratorAlreadyAssigned,
        eReplyAlreadyStarted,
        eOutputNotInReadyState,
        eRequestAlreadyPostponed,
        eRequestCannotBePostponed,
        eRequestNotPostponed,
        eConnectionClosed,
        eRequestAlreadyFinished,
        eConnectionNotAssigned,
        ePendingReqNotAssigned,
        eRequestPoolNotAvailable,
        eUnfinishedRequestNotScheduled,
        eWorkerAlreadyStarted,
        eAddressEmpty,
        ePortNotSpecified,
        eConfigurationError,
        eDaemonizationFailed,
        eNoWakeCallback,
        eNoDbPath,
        eDbFlagsError,
        eDbNotInitialized,
        eDbNotUpdated,
        eDbNotOpened,
        eIterCannotForward,
        eIterAlreadyReachedEnd,
        eIterNoPostIncrement,
        eCannotUpdateDb,
        eInvalidRequest,
        eUnknownTSEOption,
        eInvalidId2Info,
        eLogic,
        eInvalidUserRequestType,

        eAccessionMismatch,
        eVersionMismatch,
        eSeqIdMismatch,

        eInvalidPollInit,
        eInvalidPollStart,
        eInvalidTimerInit,
        eInvalidTimerStart
    };

    virtual const char *  GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CPubseqGatewayException, CException);
};



class CPubseqGatewayUVException : public CException
{
public:
    enum EErrCode {
        eUvThreadCreateFailure,
        eUvImportFailure,
        eUvListenFailure,
        eUvAsyncInitFailure,
        eUvTimerInitFailure,
        eUvKeyCreateFailure,
        eUvExportStartFailure,
        eUvExportWaitFailure
    };

    virtual const char *  GetErrCodeString(void) const;

    CPubseqGatewayUVException(const CDiagCompileInfo &  info,
                              const CException *  prev_exception,
                              EErrCode  err_code,
                              const string &  message,
                              int  uv_err_code,
                              EDiagSev  severity = eDiag_Error) :
        CException(info, prev_exception,
                   message + ": " + uv_strerror(uv_err_code), severity),
        m_UvErrCode(uv_err_code)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CPubseqGatewayUVException, CException);

public:
    int  GetUVLibraryErrorCode(void) const
    {
        return m_UvErrCode;
    }

private:
    int     m_UvErrCode;
};


END_NCBI_SCOPE

#endif
