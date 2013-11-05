#ifndef READER__HPP_INCLUDED
#define READER__HPP_INCLUDED
/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Base data reader interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <util/cache/icache.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/incr_time.hpp>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CReaderRequestResult;
class CReadDispatcher;
class CBlob_id;
class CSeq_id_Handle;
class CLoadLockSeq_ids;
class CLoadLockBlob_ids;
class CLoadLockBlob;
class CReaderCacheManager;
class CReaderAllocatedConnection;
struct SAnnotSelector;

class NCBI_XREADER_EXPORT CReader : public CObject
{
public:
    CReader(void);
    virtual ~CReader(void);

    void InitParams(CConfig& conf, const string& driver_name,
                    int default_max_conn);

    typedef unsigned TConn;
    typedef CBlob_id TBlobId;
    typedef int TBlobState;
    typedef int TBlobVersion;
    typedef int TBlobSplitVersion;
    typedef int TChunkId;
    typedef int TContentsMask;
    typedef vector<TChunkId> TChunkIds;
    typedef vector<CSeq_id_Handle> TSeqIds;
    typedef vector<CBlob_Info> TBlobIds;

    /// All LoadXxx() methods should return false if
    /// there is no requested data in the reader.
    /// This will notify dispatcher that there is no sense to retry.
    virtual bool LoadStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id) = 0;
    virtual bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel);
    virtual bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id) = 0;
    virtual bool LoadSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id);
    virtual bool LoadSeq_idAccVer(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id);
    virtual bool LoadSeq_idLabel(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id);
    virtual bool LoadSeq_idTaxId(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id);

    // bulk requests
    typedef vector<CSeq_id_Handle> TIds;
    typedef vector<bool> TLoaded;
    typedef vector<TGi> TGis;
    typedef vector<string> TLabels;
    typedef vector<int> TTaxIds;
    virtual bool LoadAccVers(CReaderRequestResult& result,
                             const TIds& ids, TLoaded& loaded, TIds& ret);
    virtual bool LoadGis(CReaderRequestResult& result,
                         const TIds& ids, TLoaded& loaded, TGis& ret);
    virtual bool LoadLabels(CReaderRequestResult& result,
                            const TIds& ids, TLoaded& loaded, TLabels& ret);
    virtual bool LoadTaxIds(CReaderRequestResult& result,
                            const TIds& ids, TLoaded& loaded, TTaxIds& ret);

    virtual bool LoadBlobVersion(CReaderRequestResult& result,
                                 const TBlobId& blob_id) = 0;
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           const string& seq_id,
                           TContentsMask mask,
                           const SAnnotSelector* sel);
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id,
                           TContentsMask mask,
                           const SAnnotSelector* sel);
    virtual bool LoadBlobs(CReaderRequestResult& result,
                           CLoadLockBlob_ids lock,
                           TContentsMask mask,
                           const SAnnotSelector* sel);
    virtual bool LoadBlob(CReaderRequestResult& result,
                          const CBlob_id& blob_id) = 0;
    virtual bool LoadBlob(CReaderRequestResult& result,
                          const CBlob_Info& blob_info);
    virtual bool LoadChunk(CReaderRequestResult& result,
                           const TBlobId& blob_id, TChunkId chunk_id);
    virtual bool LoadChunks(CReaderRequestResult& result,
                            const TBlobId& blob_id,
                            const TChunkIds& chunk_ids);
    virtual bool LoadBlobSet(CReaderRequestResult& result,
                             const TSeqIds& seq_ids);

    void SetAndSaveNoStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id,
                                   TBlobState state) const;
    void SetAndSaveNoSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id,
                                   TBlobState state) const;
    void SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                 const string& seq_id,
                                 TBlobState state,
                                 const CFixedSeq_ids& seq_ids) const;
    void SetAndSaveStringGi(CReaderRequestResult& result,
                            const string& seq_id,
                            TGi gi) const;
    void SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 TBlobState state,
                                 const CFixedSeq_ids& seq_ids) const;
    void SetAndSaveSeq_idGi(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            TGi gi) const;
    void SetAndSaveSeq_idAccVer(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id,
                                const CSeq_id& acc_id) const;
    void SetAndSaveSeq_idLabel(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            const string& label) const;
    void SetAndSaveSeq_idTaxId(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               int taxid) const;
    void SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id,
                                  const SAnnotSelector* sel,
                                  TBlobState state,
                                  const CFixedBlob_ids& blob_ids) const;
    void SetAndSaveNoSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel,
                                    TBlobState state) const;
    void SetAndSaveBlobVersion(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TBlobVersion version) const;
    void SetAndSaveNoBlob(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          TBlobState blob_state = 0);
    void SetAndSaveNoBlob(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          const CLoadLockBlob& lock);

    void SetAndSaveNoStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id,
                                   CLoadLockSeq_ids& lock,
                                   TBlobState state) const;
    void SetAndSaveNoSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id,
                                   CLoadLockSeq_ids& lock,
                                   TBlobState state) const;
    void SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                 const string& seq_id,
                                 CLoadLockSeq_ids& lock,
                                 TBlobState blob_state,
                                 const CFixedSeq_ids& seq_ids) const;
    void SetAndSaveStringGi(CReaderRequestResult& result,
                            const string& seq_id,
                            CLoadLockSeq_ids& lock,
                            TGi gi) const;
    void SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& lock,
                                 TBlobState state,
                                 const CFixedSeq_ids& seq_ids) const;
    void SetAndSaveSeq_idGi(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            CLoadLockSeq_ids& lock,
                            TGi gi) const;
    void SetAndSaveSeq_idAccVer(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id,
                                CLoadLockSeq_ids& lock,
                                const CSeq_id& acc_id) const;
    void SetAndSaveSeq_idLabel(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               CLoadLockSeq_ids& lock,
                               const string& label) const;
    void SetAndSaveSeq_idTaxId(CReaderRequestResult& result,
                               const CSeq_id_Handle& seq_id,
                               CLoadLockSeq_ids& lock,
                               int taxid) const;
    void SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id,
                                  const SAnnotSelector* sel,
                                  CLoadLockBlob_ids& lock,
                                  TBlobState state,
                                  const CFixedBlob_ids& blob_ids) const;
    void SetAndSaveNoSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel,
                                    CLoadLockBlob_ids& lock,
                                    TBlobState state) const;
    typedef CLoadLockBlob::TAnnotInfo TAnnotInfo;

    int SetMaximumConnections(int max);
    void SetMaximumConnections(int max, int default_max);
    int GetMaximumConnections(void) const;
    virtual int GetMaximumConnectionsLimit(void) const;

    void SetPreopenConnection(bool preopen = true);
    bool GetPreopenConnection(void) const;
    void OpenInitialConnection(bool force);

    // returns the time in seconds when already retrived data
    // could become obsolete by fresher version
    // -1 - never
    virtual int GetConst(const string& const_name) const;

    void SetMaximumRetryCount(int retry_count);
    virtual int GetRetryCount(void) const;
    virtual bool MayBeSkippedOnErrors(void) const;

    // CReadDispatcher can set m_Dispatcher
    friend class CReadDispatcher;

    static int ReadInt(CNcbiIstream& stream);

    virtual void InitializeCache(CReaderCacheManager& cache_manager,
                                 const TPluginManagerParamTree* params);
    virtual void ResetCache(void);

    virtual void OpenConnection(TConn conn);
    virtual void WaitBeforeNewConnection(TConn conn);
    virtual void ConnectSucceeds(TConn conn);
    virtual void ConnectFailed(TConn conn);
    virtual void SetNewConnectionDelayMicroSec(unsigned long micro_sec);

protected:
    CReadDispatcher* m_Dispatcher;

    typedef CReaderAllocatedConnection CConn;

    // allocate connection slot with key 'conn'
    virtual void x_AddConnectionSlot(TConn conn) = 0;
    // disconnect and remove connection slot with key 'conn'
    virtual void x_RemoveConnectionSlot(TConn conn) = 0;
    // disconnect at connection slot with key 'conn'
    virtual void x_DisconnectAtSlot(TConn conn, bool failed);
    // force connection at connection slot with key 'conn'
    virtual void x_ConnectAtSlot(TConn conn) = 0;

private:
    friend class CReaderAllocatedConnection;
    
    TConn x_AllocConnection(bool oldest = false);
    void x_ReleaseConnection(TConn conn, double retry_delay = 0);
    void x_ReleaseClosedConnection(TConn conn);
    void x_AbortConnection(TConn conn, bool failed);

    void x_AddConnection(void);
    void x_RemoveConnection(void);

    // parameters
    TConn            m_MaxConnections;
    bool             m_PreopenConnection;

    // current state
    TConn            m_NextNewConnection;
    struct SConnSlot {
        TConn m_Conn;
        CTime m_LastUseTime;
        double m_RetryDelay;
    };
    typedef list<SConnSlot> TFreeConnections;
    TFreeConnections m_FreeConnections;
    CMutex           m_ConnectionsMutex;
    CSemaphore       m_NumFreeConnections;
    int              m_MaximumRetryCount;
    int              m_ConnectFailCount;
    CTime            m_LastTimeFailed;
    CTime            m_NextConnectTime;
    int              m_WaitTimeErrors;
    CIncreasingTime  m_WaitTime;
};


////////////////////////////////////////////////////////////////////////////
// CConn
////////////////////////////////////////////////////////////////////////////


class NCBI_XREADER_EXPORT CReaderAllocatedConnection
{
public:
    typedef CReader::TConn TConn;

    CReaderAllocatedConnection(CReaderRequestResult& result, CReader* reader);
    ~CReaderAllocatedConnection(void);
    
    void Release(void);
    void Restart(void);

    bool IsAllocated(void) const {
        return m_Result != 0;
    }
    
    operator TConn(void) const
        {
            _ASSERT(IsAllocated());
            return m_Conn;
        }
    
private:
    CReaderRequestResult* m_Result;
    CReader* m_Reader;
    TConn m_Conn;
    bool m_Restart;
    
private:
    CReaderAllocatedConnection(const CReaderAllocatedConnection&);
    void operator=(CReaderAllocatedConnection&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER__HPP_INCLUDED
