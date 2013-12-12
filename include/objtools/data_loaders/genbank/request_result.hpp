#ifndef GBLOADER_REQUEST_RESULT__HPP_INCLUDED
#define GBLOADER_REQUEST_RESULT__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*   Class for storing results of reader's request and thread synchronization
*
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#include <util/mutex_pool.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;
class CSeq_id_Handle;
class CID2_Blob_Id;
class CID2S_Seq_annot_Info;
class CBlob_id;
class CTSE_Info;
//class CTimer;

class CGBDataLoader;
class CLoadInfo;
class CLoadInfoLock;
class CLoadLock_Base;
class CReaderRequestResult;
class CReaderRequestConn;
class CReaderAllocatedConnection;

/////////////////////////////////////////////////////////////////////////////
// resolved information classes
/////////////////////////////////////////////////////////////////////////////

class NCBI_XREADER_EXPORT CLoadInfo : public CObject
{
public:
    CLoadInfo(void);
    ~CLoadInfo(void);

    typedef Uint4 TExpirationTime; // UTC time, seconds since epoch (1970)

    bool IsLoaded(const CReaderRequestResult& rr) const;
    void SetLoaded(TExpirationTime expiration_time);
    TExpirationTime GetExpirationTime(void) const {
        return m_ExpirationTime;
    }

protected:
    void x_SetLoaded(TExpirationTime expiration_time);
    
private:
    friend class CLoadInfoLock;
    friend class CLoadLock_Base;

    CLoadInfo(const CLoadInfo&);
    CLoadInfo& operator=(const CLoadInfo&);

    volatile TExpirationTime     m_ExpirationTime;
    CInitMutex<CObject> m_LoadLock;
};


class NCBI_XREADER_EXPORT CFixedSeq_ids
{
public:
    typedef vector<CSeq_id_Handle> TList;
    typedef TList::const_reference const_reference;
    typedef TList::const_iterator const_iterator;

    CFixedSeq_ids(void);
    explicit CFixedSeq_ids(const TList& list);
    CFixedSeq_ids(ENcbiOwnership ownership, TList& list);

    const TList& Get(void) const
        {
            return m_Ref->GetData();
        }
    operator const TList&(void) const
        {
            return Get();
        }

    bool empty(void) const
        {
            return Get().empty();
        }
    size_t size(void) const
        {
            return Get().size();
        }
    const_iterator begin(void) const
        {
            return Get().begin();
        }
    const_iterator end(void) const
        {
            return Get().end();
        }
    const_reference front(void) const
        {
            return *begin();
        }

    void clear(void);

private:
    typedef CObjectFor<TList> TObject;
    CConstRef<TObject> m_Ref;
};


class NCBI_XREADER_EXPORT CLoadInfoSeq_ids : public CLoadInfo
{
public:
    typedef CSeq_id_Handle TSeq_id;
    typedef int TState;
    typedef CFixedSeq_ids TSeq_ids;

    CLoadInfoSeq_ids(void);
    CLoadInfoSeq_ids(const CSeq_id_Handle& seq_id);
    CLoadInfoSeq_ids(const string& seq_id);
    ~CLoadInfoSeq_ids(void);

    TState GetState(void) const
        {
            return m_State;
        }
    bool IsEmpty(void) const;
    TSeq_ids GetSeq_ids(void) const;

    bool SetNoSeq_ids(TState state,
                      TExpirationTime expiration_time);
    bool SetLoadedSeq_ids(TState state,
                          const CFixedSeq_ids& seq_ids,
                          TExpirationTime expiration_time);

    bool IsLoadedGi(const CReaderRequestResult& rr);
    TGi GetGi(void) const
        {
            _ASSERT(m_ExpirationTimeGi > 0);
            return m_Gi;
        }
    bool SetLoadedGi(TGi gi,
                     TExpirationTime expiration_time);

    bool IsLoadedAccVer(const CReaderRequestResult& rr);
    CSeq_id_Handle GetAccVer(void) const;
    bool SetLoadedAccVer(const CSeq_id_Handle& acc,
                         TExpirationTime expiration_time);

    bool IsLoadedLabel(const CReaderRequestResult& rr);
    string GetLabel(void) const;
    bool SetLoadedLabel(const string& label,
                        TExpirationTime expiration_time);

    bool IsLoadedTaxId(const CReaderRequestResult& rr);
    int GetTaxId(void) const
        {
            _ASSERT(m_ExpirationTimeTaxId > 0);
            return m_TaxId;
        }
    bool SetLoadedTaxId(int taxid,
                        TExpirationTime expiration_time);

private:
    TSeq_ids        m_Seq_ids;
    volatile TExpirationTime m_ExpirationTimeGi;
    volatile TExpirationTime m_ExpirationTimeAcc;
    volatile TExpirationTime m_ExpirationTimeLabel;
    volatile TExpirationTime m_ExpirationTimeTaxId;
    TGi             m_Gi;
    CSeq_id_Handle  m_Acc;
    string          m_Label;
    int             m_TaxId;
    TState          m_State;
};


class NCBI_XREADER_EXPORT CBlob_Annot_Info : public CObject
{
public:
    typedef set<string> TNamedAnnotNames;
    typedef vector< CConstRef<CID2S_Seq_annot_Info> > TAnnotInfo;
    
    const TNamedAnnotNames& GetNamedAnnotNames(void) const
        {
            return m_NamedAnnotNames;
        }

    const TAnnotInfo& GetAnnotInfo(void) const
        {
            return m_AnnotInfo;
        }

    void AddNamedAnnotName(const string& name);
    void AddAnnotInfo(const CID2S_Seq_annot_Info& info);

    bool Matches(const SAnnotSelector* sel) const;

private:
    TNamedAnnotNames    m_NamedAnnotNames;
    TAnnotInfo          m_AnnotInfo;
};


class NCBI_XREADER_EXPORT CBlob_Info
{
public:
    typedef TBlobContentsMask TContentsMask;

    CBlob_Info(void);
    CBlob_Info(CConstRef<CBlob_id> blob_id, TContentsMask contents);
    ~CBlob_Info(void);

    CConstRef<CBlob_id> GetBlob_id(void) const
        {
            return m_Blob_id;
        }
    DECLARE_OPERATOR_BOOL_REF(m_Blob_id);

    TContentsMask GetContentsMask(void) const
        {
            return m_Contents;
        }
    bool Matches(TContentsMask mask, const SAnnotSelector* sel) const;

    bool IsSetAnnotInfo(void) const
        {
            return m_AnnotInfo;
        }
    CConstRef<CBlob_Annot_Info> GetAnnotInfo(void) const
        {
            return m_AnnotInfo;
        }
    void SetAnnotInfo(CRef<CBlob_Annot_Info>& annot_info);

private:
    CConstRef<CBlob_id> m_Blob_id;
    TContentsMask       m_Contents;
    CConstRef<CBlob_Annot_Info> m_AnnotInfo;
};


class NCBI_XREADER_EXPORT CFixedBlob_ids
{
public:
    typedef vector<CBlob_Info> TList;
    typedef TList::const_reference const_reference;
    typedef TList::const_iterator const_iterator;

    CFixedBlob_ids(void);
    explicit CFixedBlob_ids(const TList& list);
    CFixedBlob_ids(ENcbiOwnership ownership, TList& list);

    const TList& Get(void) const
        {
            return m_Ref->GetData();
        }
    operator const TList&(void) const
        {
            return Get();
        }

    bool empty(void) const
        {
            return Get().empty();
        }
    size_t size(void) const
        {
            return Get().size();
        }
    const_iterator begin(void) const
        {
            return Get().begin();
        }
    const_iterator end(void) const
        {
            return Get().end();
        }
    const_reference front(void) const
        {
            return *begin();
        }

    void clear(void);

private:
    typedef CObjectFor<TList> TObject;
    CConstRef<TObject> m_Ref;
};


class NCBI_XREADER_EXPORT CLoadInfoBlob_ids : public CLoadInfo
{
public:
    typedef CSeq_id_Handle TSeq_id;
    typedef int TState;
    typedef CFixedBlob_ids TBlobIds;

    CLoadInfoBlob_ids(const TSeq_id& id, const SAnnotSelector* sel);
    CLoadInfoBlob_ids(const pair<TSeq_id, string>& key);
    ~CLoadInfoBlob_ids(void);

    const TSeq_id& GetSeq_id(void) const
        {
            return m_Seq_id;
        }

    bool IsEmpty(void) const;
    TState GetState(void) const
        {
            return m_State;
        }
    TBlobIds GetBlob_ids(void) const;

    bool SetNoBlob_ids(TState state,
                       TExpirationTime expiration_time);
    bool SetLoadedBlob_ids(TState state,
                           const TBlobIds& blob_ids,
                           TExpirationTime expiration_time);

private:
    TSeq_id     m_Seq_id;
    TBlobIds    m_Blob_ids;
    TState      m_State;
};


/////////////////////////////////////////////////////////////////////////////
// resolved information locks
/////////////////////////////////////////////////////////////////////////////

class NCBI_XREADER_EXPORT CLoadInfoLock : public CObject
{
public:
    ~CLoadInfoLock(void);

    typedef CLoadInfo::TExpirationTime TExpirationTime;

    void ReleaseLock(void);
    void SetLoaded(TExpirationTime expiration_time);
    const CLoadInfo& GetLoadInfo(void) const
        {
            return *m_Info;
        }

protected:
    friend class CReaderRequestResult;

    CLoadInfoLock(CReaderRequestResult& owner,
                  const CRef<CLoadInfo>& info);

private:
    CReaderRequestResult&   m_Owner;
    CRef<CLoadInfo>         m_Info;
    CInitGuard              m_Guard;

private:
    CLoadInfoLock(const CLoadInfoLock&);
    CLoadInfoLock& operator=(const CLoadInfoLock&);
};


class NCBI_XREADER_EXPORT CLoadLock_Base
{
public:
    typedef CLoadInfo TInfo;
    typedef CLoadInfoLock TLock;
    typedef CReaderRequestResult TMutexSource;

    typedef CLoadInfo::TExpirationTime TExpirationTime;

    bool IsLoaded(void) const
        {
            return Get().IsLoaded(*m_RequestResult);
        }
    void SetLoaded(TExpirationTime expiration_time);

    TInfo& Get(void)
        {
            return *m_Info;
        }
    const TInfo& Get(void) const
        {
            return *m_Info;
        }

    const CRef<TLock> GetLock(void) const
        {
            return m_Lock;
        }

protected:
    void Lock(TInfo& info, TMutexSource& src);

    CReaderRequestResult* m_RequestResult;

private:
    CRef<TInfo> m_Info;
    CRef<TLock> m_Lock;
};


class NCBI_XREADER_EXPORT CLoadLockBlob_ids : public CLoadLock_Base
{
public:
    typedef CLoadInfoBlob_ids TInfo;
    typedef TInfo::TState TState;
    typedef TInfo::TBlobIds TBlobIds;

    CLoadLockBlob_ids(void)
        {
        }
    CLoadLockBlob_ids(TMutexSource& src,
                      const CSeq_id_Handle& seq_id,
                      const SAnnotSelector* sel);
    CLoadLockBlob_ids(TMutexSource& src,
                      const CSeq_id_Handle& seq_id,
                      const string& na_accs);
    
    TInfo& Get(void)
        {
            return static_cast<TInfo&>(CLoadLock_Base::Get());
        }
    const TInfo& Get(void) const
        {
            return static_cast<const TInfo&>(CLoadLock_Base::Get());
        }
    TInfo& operator*(void)
        {
            return Get();
        }
    TInfo* operator->(void)
        {
            return &Get();
        }
    const TInfo& operator*(void) const
        {
            return Get();
        }
    const TInfo* operator->(void) const
        {
            return &Get();
        }

    bool SetNoBlob_ids(TState state);
    bool SetLoadedBlob_ids(TState state,
                           const TBlobIds& blob_ids);
    bool SetLoadedBlob_ids(const CLoadLockBlob_ids& ids2);

protected:
    TExpirationTime GetNewExpirationTime(void) const;

private:
    void x_Initialize(TMutexSource& src, const CSeq_id_Handle& seq_id);
};


class NCBI_XREADER_EXPORT CLoadLockSeq_ids : public CLoadLock_Base
{
public:
    typedef CLoadInfoSeq_ids TInfo;

    CLoadLockSeq_ids(TMutexSource& src, const string& seq_id);
    CLoadLockSeq_ids(TMutexSource& src, const CSeq_id_Handle& seq_id);
    CLoadLockSeq_ids(TMutexSource& src, const CSeq_id_Handle& seq_id,
                     const SAnnotSelector* sel);

    TInfo& Get(void)
        {
            return static_cast<TInfo&>(CLoadLock_Base::Get());
        }
    const TInfo& Get(void) const
        {
            return static_cast<const TInfo&>(CLoadLock_Base::Get());
        }
    TInfo& operator*(void)
        {
            return Get();
        }
    TInfo* operator->(void)
        {
            return &Get();
        }
    const TInfo& operator*(void) const
        {
            return Get();
        }
    const TInfo* operator->(void) const
        {
            return &Get();
        }

    bool SetNoSeq_ids(TInfo::TState state);
    bool SetLoadedSeq_ids(TInfo::TState state,
                          const CFixedSeq_ids& seq_ids);
    bool SetLoadedSeq_ids(const CLoadLockSeq_ids& ids2);

    bool IsLoadedGi(void)
        {
            return Get().IsLoadedGi(*m_RequestResult);
        }
    bool SetLoadedGi(TGi gi);
    bool IsLoadedAccVer(void)
        {
            return Get().IsLoadedAccVer(*m_RequestResult);
        }
    bool SetLoadedAccVer(const CSeq_id_Handle& acc);
    bool IsLoadedLabel(void)
        {
            return Get().IsLoadedLabel(*m_RequestResult);
        }
    bool SetLoadedLabel(const string& label);
    bool IsLoadedTaxId(void)
        {
            return Get().IsLoadedTaxId(*m_RequestResult);
        }
    bool SetLoadedTaxId(int taxid);

    CLoadLockBlob_ids& GetBlob_ids(void)
        {
            return m_Blob_ids;
        }

protected:
    TExpirationTime GetNewExpirationTime(void) const;

private:
    CLoadLockBlob_ids m_Blob_ids;
};


class NCBI_XREADER_EXPORT CLoadLockBlob : public CTSE_LoadLock
{
public:
    CLoadLockBlob(void);
    CLoadLockBlob(CReaderRequestResult& src, const CBlob_id& blob_id);

    typedef int TBlobState;
    typedef int TBlobVersion;
    typedef list< CRef<CID2S_Seq_annot_Info> > TAnnotInfo;

    TBlobState GetBlobState(void) const;
    void SetBlobState(TBlobState state);
    bool IsSetBlobVersion(void) const;
    TBlobVersion GetBlobVersion(void) const;
    void SetBlobVersion(TBlobVersion);

private:
    CReaderRequestResult* m_Result;
    CBlob_id m_BlobId;
};

/*
  class NCBI_XREADER_EXPORT CLoadLockBlob : public CLoadLock_Base
  {
  public:
  typedef CLoadInfoBlob TInfo;

  CLoadLockBlob(TMutexSource& src, const CBlob_id& blob_id);
    
  TInfo& Get(void)
  {
  return static_cast<TInfo&>(CLoadLock_Base::Get());
  }
  const TInfo& Get(void) const
  {
  return static_cast<const TInfo&>(CLoadLock_Base::Get());
  }
  TInfo& operator*(void)
  {
  return Get();
  }
  TInfo* operator->(void)
  {
  return &Get();
  }
  const TInfo& operator*(void) const
  {
  return Get();
  }
  const TInfo* operator->(void) const
  {
  return &Get();
  }
  };
*/


/////////////////////////////////////////////////////////////////////////////
// whole request lock
/////////////////////////////////////////////////////////////////////////////


class CReaderRequestResultRecursion;


class NCBI_XREADER_EXPORT CReaderRequestResult
{
public:
    CReaderRequestResult(const CSeq_id_Handle& requested_id);
    virtual ~CReaderRequestResult(void);

    typedef string TKeySeq_ids;
    typedef CSeq_id_Handle TKeySeq_ids2;
    typedef CLoadInfoSeq_ids TInfoSeq_ids;
    typedef pair<CSeq_id_Handle, string> TKeyBlob_ids;
    typedef CLoadInfoBlob_ids TInfoBlob_ids;
    typedef CBlob_id TKeyBlob;
    typedef CTSE_Lock TTSE_Lock;
    //typedef CLoadInfoBlob TInfoBlob;
    typedef CLoadLockBlob TLockBlob;
    typedef size_t TLevel;
    typedef int TBlobVersion;
    typedef int TBlobState;
    typedef CLoadInfo::TExpirationTime TExpirationTime;

    virtual CGBDataLoader* GetLoaderPtr(void);

    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids& seq_id) = 0;
    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids2& seq_id) = 0;
    virtual CRef<TInfoBlob_ids> GetInfoBlob_ids(const TKeyBlob_ids& seq_id) = 0;
    //virtual CRef<TInfoBlob>     GetInfoBlob(const TKeyBlob& blob_id) = 0;
    CTSE_LoadLock GetBlobLoadLock(const TKeyBlob& blob_id);
    bool IsBlobLoaded(const TKeyBlob& blob_id);

    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id) = 0;
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id) = 0;

    typedef vector<CBlob_id> TLoadedBlob_ids;
    virtual void GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                   TLoadedBlob_ids& blob_ids) const;

    bool SetBlobVersion(const TKeyBlob& blob_id, TBlobVersion version);
    bool IsSetBlobVersion(const TKeyBlob& blob_id);
    TBlobVersion GetBlobVersion(const TKeyBlob& blob_id);
    bool SetNoBlob(const TKeyBlob& blob_id, TBlobState blob_state);
    void ReleaseNotLoadedBlobs(void);

    // load ResultBlob
    //virtual CRef<CTSE_Info> GetTSE_Info(const TLockBlob& blob);
    //CRef<CTSE_Info> GetTSE_Info(const TKeyBlob& blob_id);
    //virtual void SetTSE_Info(TLockBlob& blob, const CRef<CTSE_Info>& tse);
    //void SetTSE_Info(const TKeyBlob& blob_id, const CRef<CTSE_Info>& tse);

    //bool AddTSE_Lock(const TLockBlob& blob);
    //bool AddTSE_Lock(const TKeyBlob& blob_id);
    void AddTSE_Lock(const TTSE_Lock& lock);
    typedef set<TTSE_Lock> TTSE_LockSet;
    const TTSE_LockSet& GetTSE_LockSet(void) const
        {
            return m_TSE_LockSet;
        }
    void SaveLocksTo(TTSE_LockSet& locks);

    void ReleaseLocks(void);

    //virtual TTSE_Lock AddTSE(CRef<CTSE_Info> tse, const TKeyBlob& blob_id);
    //virtual TTSE_Lock LockTSE(CRef<CTSE_Info> tse);

    virtual operator CInitMutexPool&(void) = 0;
    CRef<CLoadInfoLock> GetLoadLock(const CRef<CLoadInfo>& info);

    typedef int TConn;

    TLevel GetLevel() const { return m_Level; }
    void SetLevel(TLevel level) { m_Level = level; }
    bool IsCached() const { return m_Cached; }
    void SetCached() { m_Cached = true; }

    const CSeq_id_Handle& GetRequestedId(void) const { return m_RequestedId; }
    void SetRequestedId(const CSeq_id_Handle& requested_id);

    void ClearRetryDelay(void) { m_RetryDelay = 0; }
    void AddRetryDelay(double delay) { m_RetryDelay += delay; }
    double GetRetryDelay(void) const { return m_RetryDelay; }

    // expiration support
    TExpirationTime GetStartTime(void) const { return m_StartTime; }
    TExpirationTime GetNewIdExpirationTime(void) const;
    virtual TExpirationTime GetIdExpirationTimeout(void) const;

private:
    friend class CLoadInfoLock;
    friend class CReaderAllocatedConnection;
    friend class CReaderRequestResultRecursion;

    void ReleaseLoadLock(const CRef<CLoadInfo>& info);

    typedef map<CRef<CLoadInfo>, CRef<CLoadInfoLock> > TLockMap;
    typedef pair<TBlobVersion, CTSE_LoadLock> TBlobLoadInfo;
    typedef map<CBlob_id, TBlobLoadInfo> TBlobLoadLocks;

    TBlobLoadInfo& x_GetBlobLoadInfo(const TKeyBlob& blob_id);

    TLockMap        m_LockMap;
    TTSE_LockSet    m_TSE_LockSet;
    TBlobLoadLocks  m_BlobLoadLocks;
    TLevel          m_Level;
    bool            m_Cached;
    CSeq_id_Handle  m_RequestedId;
    int             m_RecursionLevel;
    double          m_RecursiveTime;
    CReaderAllocatedConnection* m_AllocatedConnection;
    double          m_RetryDelay;
    TExpirationTime m_StartTime;

private: // hide methods
    CReaderRequestResult(const CReaderRequestResult&);
    CReaderRequestResult& operator=(const CReaderRequestResult&);
};


class NCBI_XREADER_EXPORT CStandaloneRequestResult :
    public CReaderRequestResult
{
public:
    CStandaloneRequestResult(const CSeq_id_Handle& requested_id);
    virtual ~CStandaloneRequestResult(void);

    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids& seq_id);
    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids2& seq_id);
    virtual CRef<TInfoBlob_ids> GetInfoBlob_ids(const TKeyBlob_ids& seq_id);

    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id);

    virtual operator CInitMutexPool&(void);


    virtual TConn GetConn(void);
    virtual void ReleaseConn(void);

    void ReleaseTSE_LoadLocks();

    CInitMutexPool    m_MutexPool;

    CRef<CDataSource> m_DataSource;

    map<TKeySeq_ids, CRef<TInfoSeq_ids> >   m_InfoSeq_ids;
    map<TKeySeq_ids2, CRef<TInfoSeq_ids> >  m_InfoSeq_ids2;
    map<TKeyBlob_ids, CRef<TInfoBlob_ids> > m_InfoBlob_ids;
};


class CReaderRequestResultRecursion : public CStopWatch
{
public:
    CReaderRequestResultRecursion(CReaderRequestResult& result);
    ~CReaderRequestResultRecursion(void);
    CReaderRequestResult& GetResult(void) const
        {
            return m_Result;
        }
    // return current recursion level
    int GetRecursionLevel(void) const
        {
            return m_Result.m_RecursionLevel;
        }
    // return current request processing time within recursion
    double GetCurrentRequestTime(void) const;
private:
    CReaderRequestResult& m_Result;
    double m_SaveTime;
private: // to prevent copying
    CReaderRequestResultRecursion(const CReaderRequestResultRecursion&);
    void operator=(const CReaderRequestResultRecursion&);
};


inline
bool CLoadInfo::IsLoaded(const CReaderRequestResult& rr) const
{
    return m_LoadLock && rr.GetStartTime() <= GetExpirationTime();
}


inline
CLoadInfo::TExpirationTime
CLoadLockSeq_ids::GetNewExpirationTime(void) const
{
    return m_RequestResult->GetNewIdExpirationTime();
}


inline
CLoadInfo::TExpirationTime
CLoadLockBlob_ids::GetNewExpirationTime(void) const
{
    return m_RequestResult->GetNewIdExpirationTime();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//GBLOADER_REQUEST_RESULT__HPP_INCLUDED
