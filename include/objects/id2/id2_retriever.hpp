#ifndef OBJECTS_ID2___ID2_RETRIEVER__HPP
#define OBJECTS_ID2___ID2_RETRIEVER__HPP

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

/// @file id2_retriever.hpp
/// Interfaces for looking up data pertaining to ID2 requests.

#include <corelib/ncbimtx.hpp>
#include <objects/id2/impl/id2_retriever_types.hpp>

/** @addtogroup OBJECTS
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations.
class IID2Retriever;
class IID2RetrieverFactory;


/// IID2ReplyUpdater -- interface for updating an initially empty ID2
/// reply set corresponding to a single ID2 request.
class IID2ReplyUpdater : public CObject
{
public:
    typedef SRichID2Packet::TReplies     TReplies;
    typedef CID2_Request::TSerial_number TSerialNumber;

    /// NB: The updater will retain a reference to the metadata, for
    /// which heap allocation may therefore be in order.
    IID2ReplyUpdater(const SID2ReplyEvent& metadata);
    
    const SID2ReplyEvent& GetEventMetadata(void) const
        { return *m_Metadata; }

    /// Update (via x_UpdateReplies) and tidy replies.
    void UpdateReplies(TReplies& replies);

    /// Tidy replies to a single request.
    ///
    /// Drop any empty replies and ensure end-of-reply indicators are
    /// in the right place.
    static void TidyReplies(TReplies& replies);

    static bool HasOnlyErrors(const TReplies& replies);    

protected:
    /// Update (or populate) replies.  Invoked synchronously, so the
    /// updater should already have all needed data on hand.
    virtual void x_UpdateReplies(TReplies& replies) = 0;

private:
    CConstRef<SID2ReplyEvent> m_Metadata;
};


/// CID2ReplyAppender -- IID2ReplyUpdater implementation that simply
/// appends to an existing reply set.
///
/// Particularly well suited for main replies, for which the existing set
/// will be empty.
class CID2ReplyAppender : public IID2ReplyUpdater
{
public:
    /// NB: The appender will retain a reference to the metadata, for
    /// which heap allocation may therefore be in order.
    CID2ReplyAppender(const SID2ReplyEvent& metadata, const TReplies& replies);

protected:
    void x_UpdateReplies(TReplies& replies) override;

private:
    TReplies m_Replies;
};


/// IID2ReplyDashboard -- interface for coordinating IID2Retrievers.
///
/// Primarily implemented by CID2ReplyDashboard, but with this
/// interface factored out to allow for an alternate single-retriever
/// implementation (to facilitate proxying IID2Retriever instances as
/// CID2Processor instances).
class IID2ReplyDashboard : public CObjectEx
{
public:
    typedef IID2ReplyUpdater::TReplies   TReplies;
    typedef CID2_Request::TSerial_number TSerialNumber;

    IID2ReplyDashboard(SRichID2Packet& packet)
        : m_Packet(&packet)
        { }

    /// Add initial bookkeeping for the retriever (conservatively
    /// issuing initial status forecasts of SID2ReplyEvent::eAny for
    /// all serial numbers) and return its ID number.
    virtual int RegisterRetriever(IID2Retriever& retriever) = 0;

    /// Returns null when out of retries.
    virtual CRef<IID2Retriever> GetReplacement(int retriever_id) = 0;

    virtual void MarkRetrieverDone(int retriever_id) = 0;

    virtual void CancelRetries(int retriever_id) = 0;

    const SRichID2Packet& GetPacket(void) const
        { return *m_Packet; }

    /// The event's type should be fSynonymUpdate.
    ///
    /// Automatically unforecasts the event.
    virtual void AcceptSynonyms(const SID2ReplyEvent& event,
                                const TSeq_id_HandleSet& ids) = 0;

    /// Accept a reply updater from a retriever.
    ///
    /// Automatically unforecasts any further main or supplementary
    /// replies from the given retriever for the given serial number.
    virtual void AcceptReplyUpdater(IID2ReplyUpdater& updater) = 0;

    /// Accept an updated forecast.
    ///
    /// The forecast must be from an identified retriever, for a
    /// single request, and a (possibly improper) subset of any
    /// previous such forecast.
    virtual void AcceptStatusForecast(const SID2ReplyEvent& event) = 0;

    virtual bool IsStillNeeded(const SID2ReplyEvent& event) const = 0;

    virtual bool IsStillPlausible(const SID2ReplyEvent& event) const = 0;

    /// Returns when either a matching event has occurred or there is
    /// no further prospect of one.
    virtual void WaitForMatchingEvent(const SID2ReplyEvent& event) = 0;

protected:
    SRichID2Packet& x_UpdatePacket(void)
        { return *m_Packet; }

private:
    CRef<SRichID2Packet> m_Packet;
};


/// IID2Retriever -- interface for preparing baseline or supplementary
/// replies to ID2 requests.
class IID2Retriever : public CObject
{
public:
    typedef SRichID2Packet::TReplies     TReplies;
    typedef CID2_Request::TSerial_number TSerialNumber;

    /// How far along this retriever is.
    enum EProgress {
        eUnready, ///< Not yet ready to run.
        eReady,   ///< Ready to run, but not yet started.
        eRunning, ///< Retriever has started.
        eDone     ///< Retriever is done.
    };

    enum ERunnability {
        eRunnable_never = -1,  ///< No supported requests.
        eRunnable_maybe_later, ///< Waiting on synonym resolution or the like.
        eRunnable_now          ///< Ready to run at any point. 
    };

    /// State shared between clones, but not between other instances.
    ///
    /// Implementations are welcome to use derived classes hereof.
    struct SRetrievalContext_Base : public CObject {
        SRetrievalContext_Base(void)
            : total_clones(1)
            { }

        ///< Counting original; auto-incremented by clones' constructors.
        unsigned int total_clones;
    };

    /// Obtain a retriever ID and (thereby) conservatively issue initial
    /// status forecasts of SID2ReplyEvent::eAny for all serial numbers.
    /// (Acknowledge these forecasts locally too.)
    ///
    /// Derived classes' constructors are welcome to supply the
    /// dashboard with any specific information they can immediately
    /// provide.  However, they should hold off until x_Run on trying to
    /// establish any database or network connections.  (They likewise
    /// shouldn't take any arguments whose creation entails establishing
    /// connections.)
    IID2Retriever(IID2ReplyDashboard&     dashboard,
                  IID2RetrieverFactory&   factory,
                  SRetrievalContext_Base* context = nullptr);

    ~IID2Retriever(void) override
        { MarkDone(); }

    /// Create a clone of this fresh retriever for use in a potential
    /// (and possibly asynchronous) retry.
    ///
    /// Automatically propagate m_Events, m_Forecasts (updating the
    /// dashboard along the way), and m_Progress to the clone.
    ///
    /// Any calls to this method will immediately follow construction.
    CRef<IID2Retriever> Clone(void);

    /// Quickly assess to what extent this retriever is ready to run;
    /// issue forecast updates along the way.
    ///
    /// Implemented in terms of x_GetRunnability, always looping all the way
    /// through to avoid missing any forecast updates.
    ERunnability GetRunnability(void);

    /// Consults GetRunnability if apparently still at eUnready.
    EProgress GetProgress(void);

    /// Flags affecting (x_)Run.
    enum ERunFlags {
        /// If this retriever consults a pool of servers, reevaluate
        /// which one is best rather than using a cached preference.
        fForceRedispatch              = 1 << 0,
        /// May return ePaused if only supplementary updates remain,
        /// in which case the caller should subsequently repeat the
        /// call without this flag and it should pick up where it left
        /// off.
        fCanDeferSupplementaryUpdates = 1 << 1
    };
    DECLARE_SAFE_FLAGS_TYPE(ERunFlags, TRunFlags);

    enum ERunStatus {
        eFailed,
        ePaused,
        eSucceeded
    };

    /// Perform (or resume) retrieval and update the dashboard along
    /// the way.
    ///
    /// Returns eSucceeded if successful (where cleanly determining
    /// that this backend won't be helpful, even via retries, counts)
    /// and eFailed if the (indirect) caller should consider retrying.
    ///
    /// May instead return ePaused when (and only when) flags contains
    /// fCanDeferSupplementaryUpdates, in which case the caller should
    /// retry without that flag when ready.
    ///
    /// Typically expected to be invoked from a preexisting auxiliary
    /// thread (but might not be, hence fCanDeferSupplementaryUpdates).
    ERunStatus Run(TRunFlags flags = static_cast<TRunFlags>(0));

    /// Returns null if no retries are available and/or needed.
    CRef<IID2Retriever> GetReplacement(void);

    const IID2RetrieverFactory& GetFactory(void) const;

    /// Get freeform backend location indicator for logging purposes.
    virtual string GetEndpoint(void) const = 0;

    /// Note any parameter values of interest (beyond id2:allow's) in
    /// init.retriever_specific[x_GetTypeIndex()].
    ///
    /// Formally non-static for technical reasons, but implementors should
    /// bear in mind that typical beneficiaries will be intermittent future
    /// instances.
    virtual void PreparseInitRequest(SRichID2InitRequest& init) const
        { }

    /// Quickly acknowledge another retriever's event.
    void AcknowledgeEvent(const SID2ReplyEvent& event);

    void MarkDone(void);

    void Cancel(void)
        { MarkDone(); x_Cancel(); }

    bool IsDone(void)
        { return m_Progress == eDone; }

    bool IsRetry(void) const
        { return m_RelativeRetrieverID > 0; }

#ifndef NCBI_ENABLE_SAFE_FLAGS
protected:
#endif
    /// Flags for CheckForEvents.
    enum ECFEFlags {
        fCFE_Wait       = 1 << 0, ///< Wait (indefinitely) for a match.
        fCFE_KeepEvents = 1 << 1  ///< Keep any matching events in the queue.
    };
    DECLARE_SAFE_FLAGS_TYPE(ECFEFlags, TCFEFlags);

protected:
    typedef SID2ReplyEvent::TEventType      TEventType;
    typedef list<CConstRef<SID2ReplyEvent>> TEvents;
    typedef map<TSerialNumber, TEventType>  TForecasts;

    SRetrievalContext_Base& x_GetRetrievalContext(void);
    virtual CRef<SRetrievalContext_Base> x_NewRetrievalContext(void) const
        { return CRef<SRetrievalContext_Base>(new SRetrievalContext_Base); }

    virtual void x_CloneExtras(const IID2Retriever& original) { }

    /// Quickly assess to what extent this retriever is ready to
    /// handle the given request.
    ///
    /// Should return eRunnable_now iff either it can participate in
    /// initial synonym lookup, some already known ID is definitely of
    /// potential interest, or synonyms have come in and all are at
    /// least plausibly of interest.  (For instance, a protein-specific
    /// retriever with no further offline exclusion criteria would
    /// return true if it saw either a definite protein ID or a full
    /// synonym set with no definite nucleotide IDs.)
    ///
    /// Should return eRunnable_never (and clear forecast to eNoReply)
    /// if this retriever (and any possible clones, due to holding off
    /// until x_Run on establishing any database or network connections)
    /// definitely won't be able to handle the given request usefully,
    /// and eRunnable_maybe_later in intermediate situations.
    ///
    /// Should in general adjust forecast appropriately, bearing in mind
    /// that only tightening is allowed and that forecasts should
    /// therefore consistently err on the side of optimism.  (The first
    /// call for a given request will start with a forecast of eAny,
    /// unless the constructor has already supplied a narrower forecast.)
    virtual ERunnability x_GetRunnability(const SID2RequestWithSynonyms& req,
                                          TEventType& forecast) = 0;

    /// Perform (or resume) retrieval and update the dashboard along
    /// the way.
    ///
    /// Returns eSucceeded if successful (where cleanly determining
    /// that this backend won't be helpful, even via retries, counts)
    /// and eFailed if the (indirect) caller should consider retrying.
    ///
    /// May instead return ePaused when (and only when) flags contains
    /// fCanDeferSupplementaryUpdates, in which case the caller should
    /// retry without that flag when ready.
    ///
    /// Should check IsDone before proceeding with I/O operations
    /// (albeit taking care to leave any open connections clean).
    ///
    /// Typically expected to be invoked from a preexisting auxiliary
    /// thread (but might not be, hence fCanDeferSupplementaryUpdates).
    virtual ERunStatus x_Run(TRunFlags flags) = 0;

    virtual void x_Cancel(void) { }

    /// Internal retriever ID, mainly of use when constructing reply
    /// updaters.  (Reply dashboard otherwise exposed only via wrapper
    /// methods that automatically supply this ID.)
    int x_GetRetrieverID(void) const
        { return m_RetrieverID; }

    /// For use with SRichID2InitRequest::TRetrieverMap
    type_index x_GetTypeIndex(void) const
        { return type_index(typeid(*this)); }

    const SRichID2Packet& x_GetPacket(void) const
        { return *m_Packet; }
    
    /// Read-only view; all updates should go through x_UpdateForecast.
    const TForecasts& x_GetForecasts(void) const
        { return m_Forecasts; }

    /// Should call at most once per serial number, and only when the
    /// forecast still permits it.
    void x_SubmitSynonyms(TSerialNumber serial_number,
                          const TSeq_id_HandleSet& ids);

    /// Should call at most once per serial number, and only when the
    /// forecast still permits it.
    ///
    /// NB: The dashboard will retain a reference to the updater, for
    /// which heap allocation may therefore be in order.
    void x_SubmitReplyUpdater(IID2ReplyUpdater& updater);

    /// Sanity-check the new forecast (which must be a possibly improper
    /// subset of the previous forecast for the specified request) and
    /// pass it along to the dashboard if it's changed.
    void x_UpdateForecast(TSerialNumber serial_number, TEventType forecast);

    /// Would an event of the given type still be of interest?
    bool x_IsStillNeededHere(TSerialNumber serial_number,
                             TEventType event_type) const;

    /// Can any other retriever(s) still plausibly supply an event of
    /// the given type?
    ///
    /// NB: Any clones count as other retrievers.
    bool x_IsStillPlausibleElsewhere(TSerialNumber serial_number,
                                     TEventType event_type) const;

    /// Called automatically when GetRunnability returns eRunnable_never
    /// or x_Run returns true.
    void x_CancelRetries(void);

    /// Check for events from other retrievers.
    ///
    /// Return true iff any matches turned up.
    bool x_CheckForEvents(TEvents&      events,
                          TSerialNumber serial_number,
                          TEventType    type  = SID2ReplyEvent::eAny,
                          TCFEFlags     flags = static_cast<TCFEFlags>(0));

private:
    bool x_CheckForEvents(TEvents& events, const SID2ReplyEvent& target,
                          TCFEFlags flags);
    
    void x_CheckRunnability(void);
    
    CWeakRef<IID2ReplyDashboard> m_Dashboard; ///< weak to break cycles
    CRef<IID2RetrieverFactory>   m_Factory;
    CRef<SRetrievalContext_Base> m_Context;
    CConstRef<SRichID2Packet>    m_Packet; ///< avoid dashboard lock overhead
    const int                    m_RetrieverID;
    const unsigned int           m_RelativeRetrieverID; /// >0 for retries
    volatile TEvents             m_Events;
    CFastMutex                   m_EventsMutex;
    TForecasts                   m_Forecasts;
    volatile EProgress           m_Progress;
};

DECLARE_SAFE_FLAGS(IID2Retriever::ERunFlags);
DECLARE_SAFE_FLAGS(IID2Retriever::ECFEFlags);


/// IID2RetrieverFactory -- interface for producing IID2Retriever instances.
class IID2RetrieverFactory : public CObject
{
public:
    /// The name should be unique and stable to facilitate injection,
    /// and should not contain any asterisks, commas, or whitespace.
    virtual const string& GetName(void) const = 0;

    virtual CRef<IID2Retriever> NewRetriever(
        IID2ReplyDashboard& dashboard,
        IID2Retriever::SRetrievalContext_Base* context = nullptr) = 0;
};


//////////////////////////////////////////////////////////////////////


inline
IID2ReplyUpdater::IID2ReplyUpdater(const SID2ReplyEvent& metadata)
    : m_Metadata(&metadata)
{
    _ASSERT(metadata.event_type == SID2ReplyEvent::fErrorReply
            ||  metadata.event_type == SID2ReplyEvent::fSynonymUpdate
            ||  metadata.event_type == SID2ReplyEvent::fWeakMainReply
            ||  metadata.event_type == SID2ReplyEvent::fStrongMainReply);
    _ASSERT(metadata.serial_number > 0);
    // retriever_id may be 0 for synthetic replies (to init requests)
    _ASSERT(metadata.retriever_id >= 0);
}


inline
void IID2ReplyUpdater::UpdateReplies(TReplies& replies)
{
    if ((m_Metadata->event_type & SID2ReplyEvent::eAnyMainReply) != 0) {
        _ASSERT(replies.empty());
    }
    x_UpdateReplies(replies);
    TidyReplies(replies);
}


inline
CID2ReplyAppender::CID2ReplyAppender(const SID2ReplyEvent& metadata,
                                     const TReplies& replies)
    : IID2ReplyUpdater(metadata), m_Replies(replies)
{
#ifdef _DEBUG
    if (HasOnlyErrors(replies)) {
        _ASSERT(metadata.event_type == SID2ReplyEvent::fErrorReply);
    } else {
        _ASSERT(metadata.event_type != SID2ReplyEvent::fErrorReply);
    }
    for (const auto &it : replies) {
        _ASSERT(it->IsSetSerial_number()
                &&  it->GetSerial_number() == metadata.serial_number);
    }
    _ASSERT(replies.back()->IsSetEnd_of_reply());
#endif
}


inline
void CID2ReplyAppender::x_UpdateReplies(TReplies& replies)
{
    if ( !replies.empty() ) {
#ifdef _DEBUG
        const TSerialNumber n = GetEventMetadata().serial_number;
        for (const auto &it : replies) {
            _ASSERT(it->IsSetSerial_number()  &&  it->GetSerial_number() == n);
        }
        _ASSERT(replies.back()->IsSetEnd_of_reply());
#endif
        replies.back()->ResetEnd_of_reply();
    }
    replies.insert(replies.end(), m_Replies.begin(), m_Replies.end());
}


inline
IID2Retriever::EProgress IID2Retriever::GetProgress(void)
{
    // Defer possible indirect self-destruction (x_CheckRunnability
    // -> x_CancelRetries -> removal from dashboard)
    CRef<IID2Retriever> self_ref(this);
    if (m_Progress == eUnready) {
        x_CheckRunnability();
    }
    return m_Progress;
}


inline
const IID2RetrieverFactory& IID2Retriever::GetFactory(void) const
{
    return *m_Factory;
}


inline
void IID2Retriever::AcknowledgeEvent(const SID2ReplyEvent& event)
{
    _ASSERT(event.retriever_id > 0  &&  event.retriever_id != m_RetrieverID);
    CFastMutexGuard guard(m_EventsMutex);
    const_cast<TEvents&>(m_Events).emplace_back(&event);
}


inline
IID2Retriever::SRetrievalContext_Base& IID2Retriever::x_GetRetrievalContext()
{
    if (m_Context.Empty()) {
        m_Context = x_NewRetrievalContext();
    }
    return *m_Context;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif  /* OBJECTS_ID2___ID2_RETRIEVER__HPP */
