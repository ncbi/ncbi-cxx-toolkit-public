#ifndef PSGS_CASSPROCESSORBASE__HPP
#define PSGS_CASSPROCESSORBASE__HPP

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
 * File Description: base class for processors which may generate cassandra
 *                   fetches
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "ipsgs_processor.hpp"
#include "myncbi_callback.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


const string    kCassandraProcessorEvent = "Cassandra";
const string    kCassandraProcessorGroupName = "CASSANDRA";



class CPSGS_CassProcessorBase : public IPSGS_Processor
{
public:
    CPSGS_CassProcessorBase();
    CPSGS_CassProcessorBase(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply,
                            TProcessorPriority  priority);
    virtual ~CPSGS_CassProcessorBase();
    virtual void Cancel(void) override;
    void SignalFinishProcessing(void);
    void UnlockWaitingProcessor(void);
    void CallOnData(void);
    string GetVerboseFetches(void) const;
    void EnforceWait(void) const;
    void LoggingCallback(EDiagSev  severity, const string &  message);

protected:
    IPSGS_Processor::EPSGS_Status GetStatus(void) override;
    bool AreAllFinishedRead(void) const;
    bool IsMyNCBIFinished(void) const;
    void UpdateOverallStatus(CRequestStatus::ECode  status);
    bool IsCassandraProcessorEnabled(shared_ptr<CPSGS_Request> request) const;
    void CancelLoaders(void);
    SCass_BlobId TranslateSatToKeyspace(CBioseqInfoRecord::TSat  sat,
                                        CBioseqInfoRecord::TSatKey  sat_key,
                                        const string &  seq_id);
    bool IsTimeoutError(const string &  msg) const;
    bool IsTimeoutError(int  code) const;
    bool IsError(EDiagSev  severity) const;
    CRequestStatus::ECode  CountError(CCassFetch *  fetch_details,
                                      CRequestStatus::ECode  status,
                                      int  code,
                                      EDiagSev  severity,
                                      const string &  message,
                                      EPSGS_LoggingFlag  logging_flag,
                                      EPSGS_StatusUpdateFlag  status_update_flag);

protected:
    enum EPSGS_MyNCBILookupResult {
        ePSGS_IncludeHUPSetToNo,
        ePSGS_FoundInOKCache,
        ePSGS_FoundInErrorCache,
        ePSGS_FoundInNotFoundCache,
        ePSGS_CookieNotPresent,
        ePSGS_RequestInitiated,
        ePSGS_AddedToWaitlist
    };
    EPSGS_MyNCBILookupResult PopulateMyNCBIUser(TMyNCBIDataCB  data_cb,
                                                TMyNCBIErrorCB  error_cb);
    void ReportNoWebCubbyUser(void);
    void ReportExplicitIncludeHUPSetToNo(void);
    void ReportMyNCBIError(CRequestStatus::ECode  status,
                           const string &  my_ncbi_message);
    void ReportMyNCBINotFound(void);
    void ReportSecureSatUnauthorized(const string &  user_name);
    void ReportFailureToGetCassConnection(const string &  message);
    void ReportFailureToGetCassConnection(void);
    void CleanupMyNCBICache(void);

protected:
    // Cassandra data loaders; there could be many of them
    list<unique_ptr<CCassFetch>>    m_FetchDetails;

    bool                            m_Canceled;
    bool                            m_Unlocked;

    // The overall processor status.
    // It should not interfere the other processor statuses and be updated
    // exclusively on per processor basis.
    CRequestStatus::ECode           m_Status;

    // Populated via MyNCBI
    optional<string>                        m_UserName;
    shared_ptr<CPSG_MyNCBIRequest_WhoAmI>   m_WhoAmIRequest;
    optional<string>                        m_MyNCBICookie;
};

#endif  // PSGS_CASSPROCESSORBASE__HPP

