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

#include <ncbi_pch.hpp>

#include "pubseq_gateway_exception.hpp"


USING_NCBI_SCOPE;


const char *  CPubseqGatewayException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eRequestAlreadyStarted:
            return "eRequestAlreadyStarted";
        case eRequestCanceled:
            return "eRequestCanceled";
        case eRequestGeneratorAlreadyAssigned:
            return "eRequestGeneratorAlreadyAssigned";
        case eReplyAlreadyStarted:
            return "eReplyAlreadyStarted";
        case eOutputNotInReadyState:
            return "eOutputNotInReadyState";
        case eRequestAlreadyPostponed:
            return "eRequestAlreadyPostponed";
        case eRequestCannotBePostponed:
            return "eRequestCannotBePostponed";
        case eRequestNotPostponed:
            return "eRequestNotPostponed";
        case eRequestAlreadyFinished:
            return "eRequestAlreadyFinished";
        case eConnectionNotAssigned:
            return "eConnectionNotAssigned";
        case ePendingReqNotAssigned:
            return "ePendingReqNotAssigned";
        case eRequestPoolNotAvailable:
            return "eRequestPoolNotAvailable";
        case eUnfinishedRequestNotScheduled:
            return "eUnfinishedRequestNotScheduled";
        case eWorkerAlreadyStarted:
            return "eWorkerAlreadyStarted";
        case eAddressEmpty:
            return "eAddressEmpty";
        case ePortNotSpecified:
            return "ePortNotSpecified";
        case eConfigurationError:
            return "eConfigurationError";
        case eDaemonizationFailed:
            return "eDaemonizationFailed";
        case eNoWakeCallback:
            return "eNoWakeCallback";
        case eNoDbPath:
            return "eNoDbPath";
        case eDbFlagsError:
            return "eDbFlagsError";
        case eDbNotInitialized:
            return "eDbNotInitialized";
        case eDbNotUpdated:
            return "eDbNotUpdated";
        case eDbNotOpened:
            return "eDbNotOpened";
        case eIterCannotForward:
            return "eIterCannotForward";
        case eIterAlreadyReachedEnd:
            return "eIterAlreadyReachedEnd";
        case eIterNoPostIncrement:
            return "eIterNoPostIncrement";
        case eCannotUpdateDb:
            return "eCannotUpdateDb";
        case eInvalidRequest:
            return "eInvalidRequest";
        case eUnknownTSEOption:
            return "eUnknownTSEOption";
        case eInvalidId2Info:
            return "eInvalidId2Info";
        case eLogic:
            return "eLogic";
        case eInvalidUserRequestType:
            return "eInvalidUserRequestType";
        case eAccessionMismatch:
            return "eAccessionMismatch";
        case eVersionMismatch:
            return "eVersionMismatch";
        case eSeqIdMismatch:
            return "eSeqIdMismatch";
        case eInvalidPollInit:
            return "eInvalidPollInit";
        case eInvalidPollStart:
            return "eInvalidPollStart";
        case eInvalidTimerInit:
            return "eInvalidTimerInit";
        case eInvalidTimerStart:
            return "eInvalidTimerStart";
        case eInvalidAsyncInit:
            return "eInvalidAsyncInit";

        default:
            return CException::GetErrCodeString();
    }
}


const char *  CPubseqGatewayUVException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
        case eUvThreadCreateFailure:
            return "eUvThreadCreateFailure";
        case eUvImportFailure:
            return "eUvImportFailure";
        case eUvListenFailure:
            return "eUvListenFailure";
        case eUvAsyncInitFailure:
            return "eUvAsyncInitFailure";
        case eUvTimerInitFailure:
            return "eUvTimerInitFailure";
        case eUvKeyCreateFailure:
            return "eUvKeyCreateFailure";
        case eUvExportStartFailure:
            return "eUvExportStartFailure";
        case eUvExportWaitFailure:
            return "eUvExportWaitFailure";
        default:
            return CException::GetErrCodeString();
    }
}
