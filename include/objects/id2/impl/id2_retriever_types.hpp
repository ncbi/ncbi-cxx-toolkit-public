#ifndef OBJECTS_ID2_IMPL___ID2_RETRIEVER_TYPES__HPP
#define OBJECTS_ID2_IMPL___ID2_RETRIEVER_TYPES__HPP

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

/// @file id2_retriever_types.hpp
/// Data structures used by IID2Retriever and CID2ReplyDashboard.

#include <objects/general/User_field.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Request_Packet.hpp>

#include <typeindex>

/** @addtogroup OBJECTS
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Forward declarations.
class CID2_Request_Get_Blob_Info;
class CID2_Request_Get_Seq_id;

/// SID2RequestWithSynonyms -- single ID2 request, augmented with
/// preresolved Seq-id metadata that may facilitate reply retrieval by
/// some backends.
struct SID2RequestWithSynonyms : public CObject
{
public:
    SID2RequestWithSynonyms(const CID2_Request& req_in);
    
    CConstRef<CID2_Request> req; ///< Raw ID2 request.
    TSeq_id_HandleSet       ids; ///< Requested ID and synonyms known so far.
    CSeq_id_Handle          acc; ///< Best ID, preferably ACC.VER-style.
    /// Explicitly requested blob ID, extracted for convenience.
    /// (Won't have synonyms, but can appear in three possible places.)
    CConstRef<CID2_Blob_Id> blob_id;
    bool                    preresolved; ///< May be vacuously true.

private:
    void x_ExtractID(const CID2_Request_Get_Seq_id& gsi, const char* choice);
    void x_ExtractID(const CID2_Request_Get_Blob_Info& gbi);
    void x_ExtractID(const CID2_Blob_Id& id)
        { blob_id.Reset(&id); preresolved = true; }
};


/// SRichID2InitRequest -- single ID2 init request, augmented with
/// preparsed parameters for convenience (id2:allow values at baseline).
struct SRichID2InitRequest : public CObject
{
public:
    typedef map<string, CConstRef<CObject>> TFieldMap;
    typedef map<type_index, TFieldMap>      TRetrieverMap;
    
    SRichID2InitRequest(const CID2_Request& req);

    CConstRef<CID2_Request> raw_request;
    set<string>             id2_allow; /// id2:allow parameter values
    TRetrieverMap           retriever_specific; /// by retriever class
};


/// SRichID2Packet -- complete ID2 request packet, augmented with
/// metadata to facilitate finding individual requests and associated
/// Seq-id (CSeq_id) metadata by serial number.
///
/// This struct accommodates original packets whose requests lack
/// and/or share serial numbers by working internally in such cases
/// with formally tweaked packets with requests numbered consecutively
/// from 1, storing the original serial numbers (if any) here to allow
/// automatically substituting them back into replies and setting a
/// flag indicating that replies should come in the same order as
/// requests (even if they become available in some other order) to
/// avoid confusion.
///
/// Packets containing non-positive serial numbers likewise receive
/// internal renumbering to allow for simpler logistics, but without
/// reply order constraints.
struct SRichID2Packet : public CObject
{
    typedef CID2_Request::TSerial_number  TIndex;
    typedef list<CRef<CID2_Reply>>        TReplies;
    typedef CRef<SID2RequestWithSynonyms> TReqRef;
    typedef map<TIndex, TReqRef>          TRequests;

    /// Constructor.
    ///
    /// Assigns local serial numbers if necessary (to accommodate
    /// exotic or broken clients) but leaves synonyms empty for now.    
    SRichID2Packet(const CID2_Request_Packet& main_packet,
                   const SRichID2InitRequest* init_request_in = nullptr);

    /// Modify replies so that its contents (which should all have
    /// serial number n) have the same serial number (or lack thereof)
    /// as the corresponding original request.
    void RestoreSerialNumber(TReplies& replies, TIndex n) const;

    /// Zero-based (unlike other content); empty when no mapping needed.
    vector<CNullable<TIndex>>          orig_serial_numbers;
    CConstRef<CID2_Request_Packet>     packet; ///< Main raw ID2 req packet.
    /// Corresponding init request; may be null.
    CConstRef<SRichID2InitRequest>     init_request;
    /// The (latest) init request from this packet, if any.
    CRef<SRichID2InitRequest>          fresh_init;
    /// Indexed requests from main packet; may be replaced (atomically) by
    /// copies with details on synonyms.
    TRequests                          requests;
    SRichID2InitRequest::TRetrieverMap retriever_specific;
    bool                               force_reply_order;

private:
    /// Check whether to renumber, and whether moreover to force a
    /// specific reply order (automatically setting force_reply_order
    /// as appropriate).
    bool x_NeedRenumbering(void);
};


/// SID2ReplyEvent -- general metadata about an event taking place in
/// the course of replying to an ID2 request packet.
struct SID2ReplyEvent : public CObject
{
    enum EEventType {
        eNoReply            = 0,      ///< Definitely no reply.
        fErrorReply         = 1 << 0, ///< Just an error.
        fSynonymUpdate      = 1 << 1, ///< Synonym update for the *request*.
        fSupplementaryReply = 1 << 2, ///< Update for specific annotations.
        fWeakMainReply      = 1 << 3, ///< Baseline from a catch-all backend.
        fStrongMainReply    = 1 << 4, ///< Baseline from a specialized backend.
        eAnyMainReply       = fErrorReply | fWeakMainReply | fStrongMainReply,
        eAnyReply           = eAnyMainReply | fSupplementaryReply,
        eAny                = (fStrongMainReply << 1) - 1
    };
    DECLARE_SAFE_FLAGS_TYPE(EEventType, TEventType);
    typedef CID2_Request::TSerial_number TSerialNumber;

    SID2ReplyEvent(TEventType event_type_in, TSerialNumber serial_number_in,
                   int retriever_id_in)
        : event_type(event_type_in), serial_number(serial_number_in),
          retriever_id(retriever_id_in)
        { }
    
    /// Return true iff ALL of the following are true:
    /// - The event types overlap, OR both are eNoReply.
    /// - The serial numbers are equal, OR at least one is a wildcard.
    /// - The retriever IDs are equal, OR at least one is a wildcard.
    /// Reflexive and symmetric, but not necessarily transitive.
    bool Matches(const SID2ReplyEvent& e) const;

    static TEventType GetStrongerReplyTypes(TEventType event_type);
    
    TEventType    event_type;
    TSerialNumber serial_number; ///< 0 for any
    int           retriever_id;  ///< 0 for any, -N for any but N
};

DECLARE_SAFE_FLAGS(SID2ReplyEvent::EEventType);


END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif  /* OBJECTS_ID2_IMPL___ID2_RETRIEVER_TYPES__HPP */
