#ifndef OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP
#define OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP

/* $Id$
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   RPC client for CDD annotations
 *
 */

#include <objtools/data_loaders/cdd/cdd_access/CDD_Request_Packet.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Reply.hpp>
#include <serial/rpcbase.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

#define DEFAULT_CDD_SERVICE_NAME "getCddSeqAnnot"
#define DEFAULT_CDD_POOL_SOFT_LIMIT 10
#define DEFAULT_CDD_POOL_AGE_LIMIT 900
#define DEFAULT_CDD_EXCLUDE_NUCLEOTIDES true


class CSeq_id;
class CID2_Blob_Id;

class NCBI_CDD_ACCESS_EXPORT CCDDClient : public CRPCClient<CCDD_Request_Packet, CCDD_Reply>
{
    typedef CRPCClient<CCDD_Request_Packet, CCDD_Reply> Tparent;
public:
    /// Serial data format for requests and replies
    enum EDataFormat {
        eDefaultFormat, ///< Determined by [CCddClient] data_format
        eJSON,          ///< Bidirectional JSON
        eSemiBinary,    ///< JSON requests, binary ASN.1 replies
        eBinary         ///< Bidirectional binary ASN.1
    };
    
    // constructor
    CCDDClient(const string& service_name = kEmptyStr,
               EDataFormat data_format = eDefaultFormat);
    // destructor
    ~CCDDClient(void);

    typedef CCDD_Request_Packet TRequests;
    typedef vector< CConstRef<CCDD_Reply> > TReplies;

    virtual void Ask(const CCDD_Request_Packet& request, CCDD_Reply& reply);

    virtual void WriteRequest(CObjectOStream& out,
                              const CCDD_Request_Packet& request);

    virtual void ReadReply(CObjectIStream& in, CCDD_Reply& reply);

    const TReplies& GetReplies(void) { return m_Replies; }

    // Shortcut method for fetching a single blob-id.
    CRef<CCDD_Reply> AskBlobId(int serial_number, const CSeq_id& seq_id);

    // Shortcut method for fetching a single blob.
    CRef<CCDD_Reply> AskBlob(int serial_number, const CID2_Blob_Id& blob_id);

    void JustAsk(const CCDD_Request_Packet& request);
    void JustFetch(CCDD_Reply& reply)
        { ReadReply(*m_In, reply); }

    
private:
    // Prohibit copy constructor and assignment operator
    CCDDClient(const CCDDClient& value);
    CCDDClient& operator=(const CCDDClient& value);

    TReplies m_Replies;
    EDataFormat m_DataFormat;
};


class CCDDBlobCache;

class NCBI_CDD_ACCESS_EXPORT CCDDClientPool : public CObject
{
public:
    CCDDClientPool(const string& service_name = DEFAULT_CDD_SERVICE_NAME,
        size_t pool_soft_limit = DEFAULT_CDD_POOL_SOFT_LIMIT,
        time_t pool_age_limit = DEFAULT_CDD_POOL_AGE_LIMIT,
        bool exclude_nucleotides = DEFAULT_CDD_EXCLUDE_NUCLEOTIDES);
    ~CCDDClientPool(void);

    typedef set<CSeq_id_Handle> TSeq_idSet;
    typedef CID2_Blob_Id TBlobId;
    typedef CRef<CCDD_Reply_Get_Blob_Id> TBlobInfo;
    typedef CRef<CSeq_annot> TBlobData;

    struct SCDDBlob {
        TBlobInfo info;
        TBlobData data;
    };
    typedef SCDDBlob TBlob;

    SCDDBlob GetBlobBySeq_id(CSeq_id_Handle idh);
    SCDDBlob GetBlobBySeq_ids(const TSeq_idSet& ids);
    TBlobInfo GetBlobIdBySeq_id(CSeq_id_Handle idh);
    TBlobData GetBlobByBlobId(const TBlobId& blob_id);

    bool IsValidId(const CSeq_id& id);

    static string BlobIdToString(const TBlobId& blob_id);
    static CRef<TBlobId> StringToBlobId(const string& s);

private:
    typedef multimap<time_t, CRef<CCDDClient> > TClientPool;
    typedef TClientPool::iterator TClient;
    friend class CCDDClientGuard;

    TClient x_GetClient();
    void x_ReleaseClient(TClient& client);
    void x_DiscardClient(TClient& client);
    bool x_CheckReply(CRef<CCDD_Reply>& reply, int serial, CCDD_Reply::TReply::E_Choice choice);
    TBlobData x_RequestBlobData(const TBlobId& blob_id);

    static int x_NextSerialNumber(void);

    string              m_ServiceName;
    size_t              m_PoolSoftLimit;
    time_t              m_PoolAgeLimit;
    bool                m_ExcludeNucleotides;
    // bool                m_Compress;
    mutable CFastMutex  m_PoolLock;
    TClientPool         m_InUse;
    TClientPool         m_NotInUse;
    unique_ptr<CCDDBlobCache> m_Cache;
};

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJTOOLS_DATA_LOADERS_CDD_CDD_ACCESS__CDD_CLIENT__HPP
