#ifndef SRA__LOADER__SNP__IMPL__ID2SNP__HPP
#define SRA__LOADER__SNP__IMPL__ID2SNP__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Processor of ID2 requests for SNP data
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>
#include <sra/readers/sra/snpread.hpp>
#include <util/limited_size_map.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <vector>

BEGIN_NCBI_NAMESPACE;

class CThreadNonStop;

BEGIN_NAMESPACE(objects);

class CSeq_id;
class CDbtag;
class CID2_Request;
class CID2_Request_Packet;
class CID2_Request_Get_Seq_id;
class CID2_Request_Get_Blob_Id;
class CID2_Request_Get_Blob_Info;
class CID2_Blob_Id;
class CID2_Reply;
class CID2_Reply_Data;

class NCBI_ID2PROC_SNP_EXPORT CID2SNPProcessor_Impl : public CObject
{
public:
    explicit
    CID2SNPProcessor_Impl(const CConfig::TParamTree* params = 0,
                          const string& driver_name = kEmptyStr);
    ~CID2SNPProcessor_Impl(void);

    struct SSNPSeqInfo {
        SSNPSeqInfo(void)
            : m_IsSNP(false),
              m_ValidSNP(false)
            {
            }

        DECLARE_OPERATOR_BOOL(m_ValidSNP);

        // parameters
        string m_SNPAcc;
        CSeq_id_Handle m_SeqId;
        bool m_IsSNP;
        bool m_ValidSNP;
        // cached objects
        CSNPDb m_SNPDb;
        CSNPDbSeqIterator m_SeqIter;
        CRef<CID2_Blob_Id> m_BlobId;
    };

    typedef vector<CRef<CID2_Reply> > TReplies;
    TReplies ProcessSomeRequests(CID2_Request_Packet& packet);

    bool ProcessRequest(TReplies& replies,
                        CID2_Request& request);

    void ResetParameters(void);
    void ProcessInit(const CID2_Request& main_request);
    bool ProcessGetSeqId(TReplies& replies,
                         CID2_Request& main_request,
                         CID2_Request_Get_Seq_id& request);
    bool ProcessGetBlobId(TReplies& replies,
                          CID2_Request& main_request,
                          CID2_Request_Get_Blob_Id& request);
    bool ProcessGetBlobInfo(TReplies& replies,
                            CID2_Request& main_request,
                            CID2_Request_Get_Blob_Info& request);

protected:
    SSNPSeqInfo Resolve(TReplies& replies,
                        CID2_Request& main_request,
                        CID2_Request_Get_Seq_id& request);
    SSNPSeqInfo Resolve(TReplies& replies,
                        CID2_Request& main_request,
                        CID2_Request_Get_Blob_Id& request);

    // lookup
    SSNPSeqInfo Resolve(const CSeq_id& id);
    SSNPSeqInfo ResolveGi(TGi gi);
    SSNPSeqInfo ResolveGeneral(const CDbtag& dbtag);
    SSNPSeqInfo ResolveAcc(const CTextseq_id& id);
    typedef int TAllowSeqType;
    SSNPSeqInfo ResolveSNPAcc(const string& acc,
                              const CTextseq_id& id);
    SSNPSeqInfo ResolveProtAcc(const CTextseq_id& id);
    SSNPSeqInfo GetRootSeq(const SSNPSeqInfo& seq);
    bool IsValidRowId(SSNPSeqInfo& seq);
    bool IsCorrectVersion(SSNPSeqInfo& seq, int version);

    // get Seq-id
    CRef<CSeq_id> GetAccVer(SSNPSeqInfo& seq);
    CRef<CSeq_id> GetGeneral(SSNPSeqInfo& seq);
    TGi GetGi(SSNPSeqInfo& seq);
    void GetSeqIds(SSNPSeqInfo& seq, list<CRef<CSeq_id> >& ids);

    // get various seq info
    string GetLabel(SSNPSeqInfo& seq);
    int GetTaxId(SSNPSeqInfo& seq);
    int GetHash(SSNPSeqInfo& seq);
    TSeqPos GetLength(SSNPSeqInfo& seq);
    CSeq_inst::TMol GetType(SSNPSeqInfo& seq);
    NCBI_gb_state GetGBState(SSNPSeqInfo& seq);
    int GetID2BlobState(SSNPSeqInfo& seq);
    int GetBioseqState(SSNPSeqInfo& seq);
    CRef<CSeq_entry> GetSeq_entry(SSNPSeqInfo& seq);

    // conversion to/from blob id
    SSNPSeqInfo ResolveBlobId(const CID2_Blob_Id& id);
    CID2_Blob_Id& GetBlobId(SSNPSeqInfo& id);

    // opening SNP files (with caching)
    CSNPDb GetSNPDb(const string& prefix);
    CSNPDb& GetSNPDb(SSNPSeqInfo& seq);

    // SNP iterators
    void ResetIteratorCache(SSNPSeqInfo& seq);
    CSNPDbSeqIterator& GetContigIterator(SSNPSeqInfo& seq);
    
    void SetBlobState(CID2_Reply& main_reply,
                      int blob_state);

    bool ExcludedBlob(SSNPSeqInfo& seq,
                      const CID2_Request_Get_Blob_Info& request);
    void WriteData(const SSNPSeqInfo& seq,
                   CID2_Reply_Data& data,
                   const CSerialObject& obj);
    bool WorthCompressing(const SSNPSeqInfo& seq);
    
    typedef limited_size_map<string, CSNPDb> TSNPDbCache;

    enum ECompressData {
        eCompressData_never,
        eCompressData_some, // if it's benefitial
        eCompressData_always
    };

private:
    CMutex m_Mutex;
    CVDBMgr m_Mgr;
    TSNPDbCache m_SNPDbCache;
    ECompressData m_DefaultCompressData;
    bool m_DefaultExplicitBlobState;
    ECompressData m_CompressData;
    bool m_ExplicitBlobState;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__LOADER__SNP__IMPL__ID2SNP__HPP
