/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE
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
* Author:  Andrei Gourianov
*
* File Description:
*   Util library exceptions
*
*/

#include <ncbi_pch.hpp>
#include <util/util_exception.hpp>
#include <util/ncbi_table.hpp>
#include <util/sync_queue.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE


const char* CUtilException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNoInput:      return "eNoInput";
    case eWrongCommand: return "eWrongCommand";
    case eWrongData:    return "eWrongData";
    default:     return CException::GetErrCodeString();
    }
}

const char* CIOException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eRead:  return "eRead";
    case eWrite: return "eWrite";
    case eFlush: return "eFlush";
    case eCanceled: return "eCanceled";
    case eOverflow: return "eOverflow";
    default:     return CException::GetErrCodeString();
    }
}

const char* CEofException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eEof:  return "eEof";
    default:    return CException::GetErrCodeString();
    }
}

const char* CBlockingQueueException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eFull:     return "eFull";
    case eTimedOut: return "eTimedOut";
    default:        return CException::GetErrCodeString();
    }
}

const char* CNcbiTable_Exception::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eRowNotFound:        return "eRowNotFound";
    case eColumnNotFound:     return "eColumnNotFound";
    default:            return  CException::GetErrCodeString();
    }
}


const char* CThreadPoolException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eControllerBusy: return "eControllerBusy";
    case eTaskBusy:       return "eTaskBusy";
    case eProhibited:     return "eProhibited";
    case eInactive:       return "eInactive";
    case eInvalid:        return "eInvalid";
    default:              return CException::GetErrCodeString();
    }
}


const char* CSyncQueueException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eWrongMaxSize:       return "eWrongMaxSize";
    case eTimeout:            return "eTimeout";
    case eIterNotValid:       return "eIterNotValid";
    case eMismatchedIters:    return "eMismatchedIters";
    case eWrongGuardIter:     return "eWrongGuardIter";
    case eNoRoom:             return "eNoRoom";
    case eEmpty:              return "eEmpty";
    case eWrongInterval:      return "WrongInterval";
    case eGuardedCopy:        return "eGuardedCopy";
    default:                  return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
