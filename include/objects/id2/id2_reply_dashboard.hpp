#ifndef OBJECTS_ID2___ID2_REPLY_DASHBOARD__HPP
#define OBJECTS_ID2___ID2_REPLY_DASHBOARD__HPP

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
 * Author:  Aaron Ucko
 *
 */

/// @file id2_reply_dashboard.hpp
/// Class for tracking retrieval results (reply updaters) for an ID2
/// packet across retrievers and serial numbers.

#include <objects/id2/id2_retriever.hpp>
#include <queue>

/** @addtogroup OBJECTS
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// CID2ReplyDashboard -- class for tracking retrieval results (reply
/// updaters) for an ID2 packet across retrievers and serial numbers.
class CID2ReplyDashboard : public IID2ReplyDashboard
{
public:
    typedef list<CRef<IID2Retriever>> TRetrievers;

    /// Reason(s) for WaitForMajorEvent to return.
    enum EMajorEvents {
        fTimeout        = 1 << 0, ///< Timeout reached.
        fRetrieverReady = 1 << 1, ///< At least one retriever ready to run.
        fReplyAvailable = 1 << 2, ///< At least one complete reply available.
        fFullyReplied   = 1 << 3  ///< No more replies will be available.
    };
    DECLARE_SAFE_FLAGS_TYPE(EMajorEvents, TMajorEvents);

    /// What WaitForMajorEvent should do if it returns due to a
    /// timeout rather than due to having any replies available.
    enum ETimeoutType {
        eSoftTimeout, ///< Leave replies empty.
        /// Populate replies as fully as immediately possible,
        /// synthesizing errors for requests with no baseline replies
        /// and giving up on pending supplementary replies.
        eHardTimeout
    };

    /// Constructor.  The supplied packet may receive synonym updates.
    CID2ReplyDashboard(SRichID2Packet& packet);

    ~CID2ReplyDashboard(void) override;

    /// Hook up a retriever factory.
    ///
    /// Immediately constructs all potentially needed retriever instances.
    void UseFactory(IID2RetrieverFactory& factory, unsigned int max_tries = 1);
    
    /// Wait for a major event (possibly a timeout) to occur, and
    /// indicate which did.  (Combinations are entirely possible.)
    TMajorEvents WaitForMajorEvent(const CTimeout& timeout,
                                   ETimeoutType timeout_type);

    /// Populate replies with all reply sets that have become
    /// available since the previous call to this method (if any).
    ///
    /// For a reply set to count as available, that serial number must
    /// have either a strong baseline reply (from a specialized
    /// backend), a weak baseline reply (from a catch-all backend)
    /// with no remaining possibility of a strong one, or an error
    /// reply with no remaining possibility of a non-error baseline
    /// reply.  In addition, if there is any baseline reply, any
    /// retrievers that indicated possible supplementary replies must
    /// have either yielded them or explicitly backed down from doing
    /// so.
    ///
    /// Best called when WaitForMajorEvent just returned
    /// fReplyAvailable (possibly along with other flags).
    void GetAvailableReplies(TReplies& replies);

    /// Populate retry_sets with all retrievers that are ready for a
    /// Run call in an auxiliary thread, either freshly or for an
    /// asynchronous retry following a soft timeout.
    ///
    /// Best called when WaitForMajorEvent just returned
    /// fRetrieverReady (possibly along with other flags).
    void GetReadyRetrievers(TRetrievers& retrievers);

    // For use by retrievers:

    /// Add initial bookkeeping for the retriever (conservatively
    /// issuing initial status forecasts of SID2ReplyEvent::eAny for
    /// all serial numbers) and return its ID number.
    int RegisterRetriever(IID2Retriever& retriever) override;

    /// Returns null when out of retries.
    CRef<IID2Retriever> GetReplacement(int retriever_id) override;

    void MarkRetrieverDone(int retriever_id) override;

    void CancelRetries(int retriever_id) override;

    /// The event's type should be fSynonymUpdate.
    ///
    /// Automatically unforecasts the event.
    void AcceptSynonyms(const SID2ReplyEvent& event,
                        const TSeq_id_HandleSet& ids) override;

    /// Accept a reply updater from a retriever running in a worker thread.
    ///
    /// Automatically unforecasts any further main or supplementary
    /// replies from the given retriever for the given serial number.
    void AcceptReplyUpdater(IID2ReplyUpdater& updater) override;

    /// Accept an updated forecast.
    ///
    /// The forecast must be from an identified retriever, for a
    /// single request, and a (possibly improper) subset of any
    /// previous such forecast.
    void AcceptStatusForecast(const SID2ReplyEvent& event) override;

    bool IsStillNeeded(const SID2ReplyEvent& event) const override;

    bool IsStillPlausible(const SID2ReplyEvent& event) const override;

    /// Returns when either a matching event has occurred or there is
    /// no further prospect of one.
    void WaitForMatchingEvent(const SID2ReplyEvent& event) override;

private:
    struct SRetrieverRecord : public CObject
    {
        SRetrieverRecord(IID2Retriever& retriever_in)
            : retriever(&retriever_in),
              waiting_for(SID2ReplyEvent::eNoReply, 0, 0),
              semaphore(0, 1)
            { }
        
        CRef<IID2Retriever> retriever; ///< null when done
        SID2ReplyEvent      waiting_for;
        CSemaphore          semaphore;
        unsigned int        factory_id;
    };
    typedef vector<CRef<SRetrieverRecord>> TRetrieverRecords;

    struct SFactoryRecord : public CObject
    {
        SFactoryRecord(IID2RetrieverFactory& factory_in, int rid,
                       unsigned int n)
            : factory(&factory_in), progress(IID2Retriever::eUnready),
              first_retriever_id(rid), tries_available(n), tries_used(0)
            { }
        
        CRef<IID2RetrieverFactory> factory;
        IID2Retriever::EProgress   progress;
        unsigned int               first_retriever_id;
        unsigned int               tries_available;
        unsigned int               tries_used;
    };
    typedef vector<CRef<SFactoryRecord>> TFactoryRecords;

    enum ERequestRecordState
    {
        eRRS_incomplete, ///< Still waiting for retrievers
        eRRS_queueable,  ///< Waiting for serial number to come up in order
        eRRS_queued,     ///< Ready to send to client
        eRRS_sent        ///< Sent to client
    };
    struct SRequestRecord : public CObject
    {
        enum EIsStillX {
            eIsStillNeeded,
            eIsStillPlausible
        };

        bool IsStillX(const SID2ReplyEvent& event, EIsStillX criterion) const;
        bool IsStillIncomplete(void) const;
        /// Returns a mask of newly precluded event types.
        SID2ReplyEvent::TEventType UpdateForecastSummary(void);

        vector<SID2ReplyEvent::TEventType> forecasts; ///< combined in [0]
        CRef<IID2ReplyUpdater>             main_updater;
        vector<CRef<IID2ReplyUpdater>>     supplementary_updaters;
        ERequestRecordState                state;
        bool                               had_synonym_updates;
    };
    typedef map<TSerialNumber, CRef<SRequestRecord>> TRequestRecords;

    /// Expects to have a write lock already.
    void x_NotifyRetrievers(const SID2ReplyEvent& event);

    /// Expects to have a write lock already.
    void x_NotifyOfMajorEvents(void);

    CRef<CID2_Reply> x_NewErrorReply(TSerialNumber serial_number) const;

    /// Expects to have a write lock already.
    CRef<IID2Retriever> x_GetRetriever(SFactoryRecord& fr,
                                       IID2Retriever::EProgress max_progress);

    /// Expects to have a write lock already.
    void x_QueueIfNewlyComplete(TRequestRecords::iterator it);

    bool x_IsStillX(const SID2ReplyEvent& event,
                    SRequestRecord::EIsStillX criterion) const;

    /// Expects to have at least a read lock already.
    bool x_HasMajorEvent(void) const
        {
            return (const_cast<const TMajorEvents&>(m_MajorEvents)
                    != static_cast<TMajorEvents>(0));
        }
    
    TRetrieverRecords     m_RetrieverRecords; ///< [0] unused
    TFactoryRecords       m_FactoryRecords;
    TRequestRecords       m_RequestRecords;
    queue<TSerialNumber>  m_ReadyToSend;
    size_t                m_OutstandingRequestCount;
    volatile TMajorEvents m_MajorEvents;
    mutable CRWLock       m_RWLock;
    CSemaphore            m_Semaphore;
};

DECLARE_SAFE_FLAGS(CID2ReplyDashboard::EMajorEvents);


//////////////////////////////////////////////////////////////////////


inline
bool CID2ReplyDashboard::IsStillNeeded(const SID2ReplyEvent& event) const
{
    return x_IsStillX(event, SRequestRecord::eIsStillNeeded);
}


inline
bool CID2ReplyDashboard::IsStillPlausible(const SID2ReplyEvent& event) const
{
    return x_IsStillX(event, SRequestRecord::eIsStillPlausible);
}


inline
SID2ReplyEvent::TEventType
CID2ReplyDashboard::SRequestRecord::UpdateForecastSummary(void)
{
    auto old_summary = forecasts[0], summary = forecasts[1];
    for (size_t n = 2;  n < forecasts.size();  ++n) {
        summary |= forecasts[n];
    }
    forecasts[0] = summary;
    return old_summary & ~summary;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif  /* OBJECTS_ID2___ID2_REPLY_DASHBOARD__HPP */
