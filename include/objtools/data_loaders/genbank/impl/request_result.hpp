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
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/impl/info_cache.hpp>

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
class CTSE_SetObjectInfo;

class CGBDataLoader;
class CReadDispatcher;
class CReaderRequestResult;
class CReaderRequestConn;
class CReaderAllocatedConnection;
class CWriter;

enum EAlreadyLoaded {
    eAlreadyLoaded
};

class NCBI_XREADER_EXPORT CFixedSeq_ids
{
public:
    typedef int TState;
    typedef vector<CSeq_id_Handle> TList;
    typedef TList::const_reference const_reference;
    typedef TList::const_iterator const_iterator;

    enum {
        kUnknownState = -256
    };
    explicit CFixedSeq_ids(TState state = kUnknownState);
    explicit CFixedSeq_ids(const TList& list, TState state = kUnknownState);
    CFixedSeq_ids(ENcbiOwnership ownership, TList& list, TState state = kUnknownState);

    const TList& Get(void) const
        {
            return m_Ref->GetData();
        }
    operator const TList&(void) const
        {
            return Get();
        }

    TState GetState(void) const
        {
            return m_State;
        }
    void SetState(TState state)
        {
            m_State = state;
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

    TGi FindGi(void) const;
    CSeq_id_Handle FindAccVer(void) const;
    string FindLabel(void) const;

private:
    typedef CObjectFor<TList> TObject;

    TState m_State;
    CConstRef<TObject> m_Ref;
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

    const CConstRef<CBlob_id>& GetBlob_id(void) const
        {
            return m_Blob_id;
        }
    DECLARE_OPERATOR_BOOL_REF(m_Blob_id);

    const TContentsMask& GetContentsMask(void) const
        {
            return m_Contents;
        }
    bool Matches(TContentsMask mask, const SAnnotSelector* sel) const;

    bool IsSetAnnotInfo(void) const
        {
            return m_AnnotInfo.NotEmpty();
        }
    const CConstRef<CBlob_Annot_Info>& GetAnnotInfo(void) const
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
    typedef int TState;
    typedef vector<CBlob_Info> TList;
    typedef TList::const_reference const_reference;
    typedef TList::const_iterator const_iterator;

    enum {
        kUnknownState = -256
    };
    explicit CFixedBlob_ids(TState state = kUnknownState);
    explicit CFixedBlob_ids(const TList& list, TState state = kUnknownState);
    CFixedBlob_ids(ENcbiOwnership ownership, TList& list, TState state = kUnknownState);

    TState GetState(void) const
        {
            return m_State;
        }
    void SetState(TState state)
        {
            m_State = state;
        }

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

private:
    typedef CObjectFor<TList> TObject;

    TState m_State;
    CConstRef<TObject> m_Ref;
};


/////////////////////////////////////////////////////////////////////////////
// whole request lock
/////////////////////////////////////////////////////////////////////////////


class CReaderRequestResultRecursion;


class NCBI_XREADER_EXPORT CGBInfoManager : public GBL::CInfoManager
{
public:
    CGBInfoManager(size_t gc_size);
    ~CGBInfoManager(void);

    typedef GBL::CInfoCache<CSeq_id_Handle, CSeq_id_Handle> TCacheAcc;
    typedef GBL::CInfoCache<CSeq_id_Handle, CFixedSeq_ids> TCacheSeqIds;
    typedef GBL::CInfoCache<CSeq_id_Handle, TGi> TCacheGi;
    typedef GBL::CInfoCache<string, CFixedSeq_ids> TCacheStrSeqIds;
    typedef GBL::CInfoCache<string, TGi> TCacheStrGi;
    typedef GBL::CInfoCache<CSeq_id_Handle, string> TCacheLabel;
    typedef int TTaxId;
    typedef GBL::CInfoCache<CSeq_id_Handle, TTaxId> TCacheTaxId;
    typedef pair<CSeq_id_Handle, string> TKeyBlobIds;
    typedef GBL::CInfoCache<TKeyBlobIds, CFixedBlob_ids> TCacheBlobIds;
    typedef int TBlobState;
    typedef GBL::CInfoCache<CBlob_id, TBlobState> TCacheBlobState;
    typedef int TSequenceHash;
    typedef GBL::CInfoCache<CSeq_id_Handle, TSequenceHash> TCacheHash;
    typedef TSeqPos TSequenceLength;
    typedef GBL::CInfoCache<CSeq_id_Handle, TSequenceLength> TCacheLength;
    typedef CSeq_inst::EMol TSequenceType;
    typedef GBL::CInfoCache<CSeq_id_Handle, TSequenceType> TCacheType;
    typedef int TBlobVersion;
    typedef GBL::CInfoCache<CBlob_id, TBlobVersion> TCacheBlobVersion;
    typedef GBL::CInfoCache<CBlob_id, CTSE_LoadLock> TCacheBlob;
    typedef int TChunkId;

    TCacheAcc m_CacheAcc;
    TCacheSeqIds m_CacheSeqIds;
    TCacheGi m_CacheGi;
    TCacheStrSeqIds m_CacheStrSeqIds;
    TCacheStrGi m_CacheStrGi;
    TCacheLabel m_CacheLabel;
    TCacheTaxId m_CacheTaxId;
    TCacheHash m_CacheHash;
    TCacheLength m_CacheLength;
    TCacheType m_CacheType;
    TCacheBlobIds m_CacheBlobIds;
    TCacheBlobState m_CacheBlobState;
    TCacheBlobVersion m_CacheBlobVersion;
    TCacheBlob m_CacheBlob;
};


class NCBI_XREADER_EXPORT CLoadLockSeqIds :
    public CGBInfoManager::TCacheSeqIds::TInfoLock
{
    typedef CGBInfoManager::TCacheSeqIds::TInfoLock TParent;
public:
    CLoadLockSeqIds(void)
        {
        }
    CLoadLockSeqIds(CReaderRequestResult& result, const CSeq_id_Handle& id);
    CLoadLockSeqIds(CReaderRequestResult& result, const string& id);
    CLoadLockSeqIds(CReaderRequestResult& result, const CSeq_id_Handle& id,
                    EAlreadyLoaded);
    CLoadLockSeqIds(CReaderRequestResult& result, const string& id,
                    EAlreadyLoaded);

    TData GetSeq_ids(void) const
        {
            return GetData();
        }
    TData::TState GetState(void) const
        {
            return GetData().GetState();
        }
    bool SetLoadedSeq_ids(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedSeq_ids(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
    bool SetLoadedSeq_ids(const CLoadLockSeqIds& ids)
        {
            return SetLoaded(ids.GetSeq_ids(), ids.GetExpirationTime());
        }
};


class NCBI_XREADER_EXPORT CLoadLockGi :
    public CGBInfoManager::TCacheGi::TInfoLock
{
    typedef CGBInfoManager::TCacheGi::TInfoLock TParent;
public:
    CLoadLockGi(void)
        {
        }
    CLoadLockGi(CReaderRequestResult& result, const CSeq_id_Handle& id);
    CLoadLockGi(CReaderRequestResult& result, const string& id);

    bool IsLoadedGi(void) const
        {
            return IsLoaded();
        }
    TExpirationTime GetExpirationTimeGi(void) const
        {
            return GetExpirationTime();
        }
    TData GetGi(void) const
        {
            return GetData();
        }
    bool SetLoadedGi(TGi data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedGi(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockAcc :
    public CGBInfoManager::TCacheAcc::TInfoLock
{
    typedef CGBInfoManager::TCacheAcc::TInfoLock TParent;
public:
    CLoadLockAcc(void)
        {
        }
    CLoadLockAcc(CReaderRequestResult& result, const CSeq_id_Handle& id);
    CLoadLockAcc(CReaderRequestResult& result, const string& id);

    bool IsLoadedAccVer(void) const
        {
            return IsLoaded();
        }
    TExpirationTime GetExpirationTimeAcc(void) const
        {
            return GetExpirationTime();
        }
    TData GetAccVer(void) const
        {
            return GetData();
        }
    bool SetLoadedAccVer(const CSeq_id_Handle& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedAccVer(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockLabel :
    public CGBInfoManager::TCacheLabel::TInfoLock
{
    typedef CGBInfoManager::TCacheLabel::TInfoLock TParent;
public:
    CLoadLockLabel(void)
        {
        }
    CLoadLockLabel(CReaderRequestResult& result, const CSeq_id_Handle& id);

    bool IsLoadedLabel(void) const
        {
            return IsLoaded();
        }
    TData GetLabel(void) const
        {
            return GetData();
        }
    bool SetLoadedLabel(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedLabel(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockTaxId :
    public CGBInfoManager::TCacheTaxId::TInfoLock
{
    typedef CGBInfoManager::TCacheTaxId::TInfoLock TParent;
public:
    CLoadLockTaxId(void)
        {
        }
    CLoadLockTaxId(CReaderRequestResult& result, const CSeq_id_Handle& id);

    bool IsLoadedTaxId(void) const
        {
            return IsLoaded();
        }
    TData GetTaxId(void) const
        {
            return GetData();
        }
    bool SetLoadedTaxId(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedTaxId(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockHash :
    public CGBInfoManager::TCacheHash::TInfoLock
{
    typedef CGBInfoManager::TCacheHash::TInfoLock TParent;
public:
    CLoadLockHash(void)
        {
        }
    CLoadLockHash(CReaderRequestResult& result, const CSeq_id_Handle& id);

    bool IsLoadedHash(void) const
        {
            return IsLoaded();
        }
    TData GetHash(void) const
        {
            return GetData();
        }
    bool SetLoadedHash(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedHash(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockLength :
    public CGBInfoManager::TCacheLength::TInfoLock
{
    typedef CGBInfoManager::TCacheLength::TInfoLock TParent;
public:
    CLoadLockLength(void)
        {
        }
    CLoadLockLength(CReaderRequestResult& result, const CSeq_id_Handle& id);

    bool IsLoadedLength(void) const
        {
            return IsLoaded();
        }
    TData GetLength(void) const
        {
            return GetData();
        }
    bool SetLoadedLength(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedLength(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockType :
    public CGBInfoManager::TCacheType::TInfoLock
{
    typedef CGBInfoManager::TCacheType::TInfoLock TParent;
public:
    CLoadLockType(void)
        {
        }
    CLoadLockType(CReaderRequestResult& result, const CSeq_id_Handle& id);

    bool IsLoadedType(void) const
        {
            return IsLoaded();
        }
    TData GetType(void) const
        {
            return GetData();
        }
    bool SetLoadedType(const TData& data)
        {
            return SetLoaded(data);
        }
    bool SetLoadedType(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
};


class NCBI_XREADER_EXPORT CLoadLockBlobIds :
    public CGBInfoManager::TCacheBlobIds::TInfoLock
{
    typedef CGBInfoManager::TCacheBlobIds::TInfoLock TParent;
public:
    CSeq_id_Handle m_Seq_id;
    CLoadLockBlobIds(void)
        {
        }
    CLoadLockBlobIds(CReaderRequestResult& result,
                     const CSeq_id_Handle& id, const SAnnotSelector* sel = 0);
    CLoadLockBlobIds(CReaderRequestResult& result,
                     const CSeq_id_Handle& id, const SAnnotSelector* sel,
                     EAlreadyLoaded);

    TData GetBlob_ids(void) const
        {
            return GetData();
        }
    TData::TState GetState(void) const
        {
            return GetData().GetState();
        }
    bool SetLoadedBlob_ids(const TData& data, TExpirationTime expiration_time)
        {
            return SetLoaded(data, expiration_time);
        }
    bool SetLoadedBlob_ids(const TData& data)
        {
            return SetLoadedBlob_ids(data, GetNewExpirationTime());
        }
    bool SetNoBlob_ids(const CFixedBlob_ids::TState& state,
                       TExpirationTime expiration_time)
        {
            return SetLoadedBlob_ids(CFixedBlob_ids(state), expiration_time);
        }
    bool SetNoBlob_ids(const CFixedBlob_ids::TState& state)
        {
            return SetLoadedBlob_ids(CFixedBlob_ids(state));
        }
    bool SetLoadedBlob_ids(const CLoadLockBlobIds& ids)
        {
            return SetLoadedBlob_ids(ids.GetBlob_ids(), ids.GetExpirationTime());
        }
};


class NCBI_XREADER_EXPORT CLoadLockBlobState :
    public CGBInfoManager::TCacheBlobState::TInfoLock
{
    typedef CGBInfoManager::TCacheBlobState::TInfoLock TParent;
public:
    CLoadLockBlobState(void)
        {
        }
    CLoadLockBlobState(CReaderRequestResult& result, const CBlob_id& id);
    CLoadLockBlobState(CReaderRequestResult& result, const CBlob_id& id,
                       EAlreadyLoaded);

    TData GetBlobState(void) const
        {
            return GetData();
        }
    bool IsLoadedBlobState(void) const
        {
            return IsLoaded();
        }
};


class NCBI_XREADER_EXPORT CLoadLockBlobVersion :
    public CGBInfoManager::TCacheBlobVersion::TInfoLock
{
    typedef CGBInfoManager::TCacheBlobVersion::TInfoLock TParent;
public:
    CLoadLockBlobVersion(void)
        {
        }
    CLoadLockBlobVersion(CReaderRequestResult& result, const CBlob_id& id);
    CLoadLockBlobVersion(CReaderRequestResult& result, const CBlob_id& id,
                         EAlreadyLoaded);

    TData GetBlobVersion(void) const
        {
            return GetData();
        }
    bool IsLoadedBlobVersion(void) const
        {
            return IsLoaded();
        }
};


class CLoadLockBlob;
class CLoadLockSetter;

// CLoadLockBlob performs pre-loading lock and check
// CLoadLockSetter should be used to do pass loaded data to OM 
class NCBI_XREADER_EXPORT CLoadLockBlob :
    public CGBInfoManager::TCacheBlob::TInfoLock
{
    typedef CGBInfoManager::TCacheBlob::TInfoLock TParent;
public:
    typedef CGBInfoManager::TChunkId TChunkId;
    typedef CGBInfoManager::TBlobVersion TBlobVersion;

    CLoadLockBlob(CReaderRequestResult& src,
                  const CBlob_id& blob_id,
                  TChunkId chunk_id = kMain_ChunkId); // initial chunk
    ~CLoadLockBlob(void);

    bool IsLoadedBlob(void) const // test if main blob is loaded
        {
            return TParent::IsLoaded();
        }

    void SelectChunk(TChunkId chunk_id); // select current chunk
    TChunkId GetSelectedChunkId(void) const;

    bool IsLoadedChunk(void) const; // test current chunk
    bool IsLoadedChunk(TChunkId chunk_id) const; // test another chunk

    CTSE_LoadLock& GetTSE_LoadLock(void);
    const CTSE_Chunk_Info& GetTSE_Chunk_Info(void)
        {
            return *m_Chunk;
        }

    TBlobVersion GetKnownBlobVersion(void) const;

    const CTSE_Split_Info& GetSplitInfo(void) const;
    bool NeedsDelayedMainChunk(void) const;

protected:
    friend class CLoadLockSetter;

    void x_ObtainTSE_LoadLock(CReaderRequestResult& result);

private:
    // hide non-specific methods
    // use either Blob or Chunk variants
    void IsLoaded(void);
    void SetLoaded(void);

    CBlob_id                m_Blob_id;
    CTSE_LoadLock           m_TSE_LoadLock; // after loading for chunk access
    CConstRef<CTSE_Chunk_Info> m_Chunk; // initial chunk
};


// CLoadLockSetter should be used to do pass loaded data to OM 
class NCBI_XREADER_EXPORT CLoadLockSetter :
    public CGBInfoManager::TCacheBlob::TInfoLock
{
    typedef CGBInfoManager::TCacheBlob::TInfoLock TParent;
public:
    typedef CGBInfoManager::TChunkId TChunkId;
    typedef CGBInfoManager::TBlobState TBlobState;
    typedef CGBInfoManager::TBlobVersion TBlobVersion;

    explicit CLoadLockSetter(CLoadLockBlob& blob);
    CLoadLockSetter(CLoadLockBlob& blob, TChunkId chunk_id);
    CLoadLockSetter(CReaderRequestResult& src,
                    const CBlob_id& blob_id,
                    TChunkId chunk_id = kMain_ChunkId);
    ~CLoadLockSetter(void);

    bool IsLoaded(void) const;
    void SetLoaded(void);

    CTSE_LoadLock& GetTSE_LoadLock(void)
        {
            return m_TSE_LoadLock;
        }
    CTSE_Chunk_Info& GetTSE_Chunk_Info(void)
        {
            return *m_Chunk;
        }

    CTSE_Split_Info& GetSplitInfo(void);

    TBlobState GetBlobState(void) const;

    void SetSeq_entry(CSeq_entry& entry, CTSE_SetObjectInfo* set_info = 0);

protected:

    void x_SelectChunk(TChunkId chunk_id);
    void x_Init(CLoadLockBlob& blob, TChunkId chunk_id);
    void x_ObtainTSE_LoadLock(CReaderRequestResult& result,
                              const CBlob_id& blob_id);

private:
    CTSE_LoadLock   m_TSE_LoadLock;
    CRef<CTSE_Chunk_Info> m_Chunk;
};


class NCBI_XREADER_EXPORT CReaderRequestResult : public GBL::CInfoRequestor
{
public:
    CReaderRequestResult(const CSeq_id_Handle& requested_id,
                         CReadDispatcher& dispatcher, // for getting CWriter
                         CGBInfoManager& manager);
    virtual ~CReaderRequestResult(void);

    CGBInfoManager& GetGBInfoManager(void)
        {
            return static_cast<CGBInfoManager&>(CInfoRequestor::GetManager());
        }

    typedef string TKeySeq_ids;
    typedef CSeq_id_Handle TKeySeq_ids2;
    typedef pair<CSeq_id_Handle, string> TKeyBlob_ids;
    typedef CBlob_id TKeyBlobState;
    typedef CBlob_id TKeyBlob;
    typedef size_t TLevel;
    typedef CGBInfoManager::TBlobVersion TBlobVersion;
    typedef CGBInfoManager::TBlobState TBlobState;
    typedef GBL::CInfo_Base::TExpirationTime TExpirationTime;

    //////////////////////////////////////////////////////////////////
    // new lock Manager support

    TExpirationTime GetRequestTime(void) const;
    TExpirationTime GetNewExpirationTime(void) const;

    typedef CGBInfoManager::TCacheSeqIds::TInfoLock TInfoLockIds;
    typedef CGBInfoManager::TCacheAcc::TInfoLock TInfoLockAcc;
    typedef CGBInfoManager::TCacheGi::TInfoLock TInfoLockGi;
    typedef CGBInfoManager::TCacheLabel::TInfoLock TInfoLockLabel;
    typedef CGBInfoManager::TTaxId TTaxId;
    typedef CGBInfoManager::TCacheTaxId::TInfoLock TInfoLockTaxId;
    typedef CGBInfoManager::TSequenceHash TSequenceHash;
    typedef CGBInfoManager::TCacheHash::TInfoLock TInfoLockHash;
    typedef CGBInfoManager::TSequenceLength TSequenceLength;
    typedef CGBInfoManager::TCacheLength::TInfoLock TInfoLockLength;
    typedef CGBInfoManager::TSequenceType TSequenceType;
    typedef CGBInfoManager::TCacheType::TInfoLock TInfoLockType;
    typedef CGBInfoManager::TCacheBlobIds::TInfoLock TInfoLockBlobIds;
    typedef CGBInfoManager::TCacheBlobState::TInfoLock TInfoLockBlobState;
    typedef CGBInfoManager::TCacheBlobVersion::TInfoLock TInfoLockBlobVersion;
    typedef CGBInfoManager::TCacheBlob::TInfoLock TInfoLockBlob;
    
    bool IsLoadedSeqIds(const CSeq_id_Handle& id);
    bool MarkLoadingSeqIds(const CSeq_id_Handle& id);
    TInfoLockIds GetLoadLockSeqIds(const CSeq_id_Handle& id);
    TInfoLockIds GetLoadedSeqIds(const CSeq_id_Handle& id);
    bool SetLoadedSeqIds(const CSeq_id_Handle& id,
                         const CFixedSeq_ids& value);

    bool IsLoadedAcc(const CSeq_id_Handle& id);
    bool MarkLoadingAcc(const CSeq_id_Handle& id);
    TInfoLockAcc GetLoadLockAcc(const CSeq_id_Handle& id);
    TInfoLockAcc GetLoadedAcc(const CSeq_id_Handle& id);
    bool SetLoadedAcc(const CSeq_id_Handle& id,
                      const CSeq_id_Handle& value);

    bool IsLoadedGi(const CSeq_id_Handle& id);
    bool MarkLoadingGi(const CSeq_id_Handle& id);
    TInfoLockGi GetLoadLockGi(const CSeq_id_Handle& id);
    TInfoLockGi GetLoadedGi(const CSeq_id_Handle& id);
    bool SetLoadedGi(const CSeq_id_Handle& id,
                     const TGi& value);

    bool IsLoadedSeqIds(const string& id);
    bool MarkLoadingSeqIds(const string& id);
    TInfoLockIds GetLoadLockSeqIds(const string& id);
    TInfoLockIds GetLoadedSeqIds(const string& id);
    bool SetLoadedSeqIds(const string& id,
                         const CFixedSeq_ids& value);

    bool IsLoadedGi(const string& id);
    bool MarkLoadingGi(const string& id);
    TInfoLockGi GetLoadLockGi(const string& id);
    TInfoLockGi GetLoadedGi(const string& id);
    bool SetLoadedGi(const string& id,
                     const TGi& value);

    // set Acc.Ver info using Ids info
    bool UpdateAccFromSeqIds(TInfoLockAcc& acc_lock,
                             const TInfoLockIds& ids_lock);
    bool SetLoadedAccFromSeqIds(const CSeq_id_Handle& id,
                                const CLoadLockSeqIds& ids_lock);

    // set GI info using Ids info
    bool UpdateGiFromSeqIds(TInfoLockGi& gi_lock,
                            const TInfoLockIds& ids_lock);
    bool SetLoadedGiFromSeqIds(const CSeq_id_Handle& id,
                               const CLoadLockSeqIds& ids_lock);

    // set Ids info using other Ids info
    bool SetLoadedSeqIds(const CSeq_id_Handle& id,
                         const CLoadLockSeqIds& ids);
    bool SetLoadedSeqIdsFromZeroGi(const CSeq_id_Handle& id,
                                   const CLoadLockGi& gi_lock);
    bool SetLoadedBlobIds(const CSeq_id_Handle& id,
                          const SAnnotSelector* sel,
                          const CLoadLockBlobIds& ids);
    bool SetLoadedBlobIdsFromZeroGi(const CSeq_id_Handle& id,
                                    const SAnnotSelector* sel,
                                    const CLoadLockGi& gi_lock);

    // set Acc.Ver info using Ids info
    bool UpdateLabelFromSeqIds(TInfoLockLabel& label_lock,
                               const TInfoLockIds& ids_lock);
    bool SetLoadedLabelFromSeqIds(const CSeq_id_Handle& id,
                                  const CLoadLockSeqIds& ids_lock);

    bool IsLoadedLabel(const CSeq_id_Handle& id);
    bool MarkLoadingLabel(const CSeq_id_Handle& id);
    TInfoLockLabel GetLoadLockLabel(const CSeq_id_Handle& id);
    TInfoLockLabel GetLoadedLabel(const CSeq_id_Handle& id);
    bool SetLoadedLabel(const CSeq_id_Handle& id,
                        const string& value);

    bool IsLoadedTaxId(const CSeq_id_Handle& id);
    bool MarkLoadingTaxId(const CSeq_id_Handle& id);
    TInfoLockTaxId GetLoadLockTaxId(const CSeq_id_Handle& id);
    TInfoLockTaxId GetLoadedTaxId(const CSeq_id_Handle& id);
    bool SetLoadedTaxId(const CSeq_id_Handle& id,
                        const TTaxId& value);

    bool IsLoadedHash(const CSeq_id_Handle& id);
    bool MarkLoadingHash(const CSeq_id_Handle& id);
    TInfoLockHash GetLoadLockHash(const CSeq_id_Handle& id);
    TInfoLockHash GetLoadedHash(const CSeq_id_Handle& id);
    bool SetLoadedHash(const CSeq_id_Handle& id,
                       const TSequenceHash& value);

    bool IsLoadedLength(const CSeq_id_Handle& id);
    bool MarkLoadingLength(const CSeq_id_Handle& id);
    TInfoLockLength GetLoadLockLength(const CSeq_id_Handle& id);
    TInfoLockLength GetLoadedLength(const CSeq_id_Handle& id);
    bool SetLoadedLength(const CSeq_id_Handle& id,
                       const TSequenceLength& value);

    bool IsLoadedType(const CSeq_id_Handle& id);
    bool MarkLoadingType(const CSeq_id_Handle& id);
    TInfoLockType GetLoadLockType(const CSeq_id_Handle& id);
    TInfoLockType GetLoadedType(const CSeq_id_Handle& id);
    bool SetLoadedType(const CSeq_id_Handle& id,
                       const TSequenceType& value);

    bool IsLoadedBlobIds(const CSeq_id_Handle& id,
                         const SAnnotSelector* sel);
    bool MarkLoadingBlobIds(const CSeq_id_Handle& id,
                            const SAnnotSelector* sel);
    TInfoLockBlobIds GetLoadLockBlobIds(const CSeq_id_Handle& id,
                                        const SAnnotSelector* sel);
    TInfoLockBlobIds GetLoadedBlobIds(const CSeq_id_Handle& id,
                                      const SAnnotSelector* sel);
    bool SetLoadedBlobIds(const CSeq_id_Handle& id,
                          const SAnnotSelector* sel,
                          const CFixedBlob_ids& value);

    bool IsLoadedBlobState(const TKeyBlob& blob_id);
    bool MarkLoadingBlobState(const TKeyBlob& blob_id);
    TInfoLockBlobState GetLoadLockBlobState(const TKeyBlob& blob_id);
    TInfoLockBlobState GetLoadedBlobState(const TKeyBlob& blob_id);
    bool SetLoadedBlobState(const TKeyBlob& blob_id, TBlobState state);
    void SetAndSaveBlobState(const TKeyBlob& blob_id,
                             TBlobState state);

    bool IsLoadedBlobVersion(const TKeyBlob& blob_id);
    bool MarkLoadingBlobVersion(const TKeyBlob& blob_id);
    TInfoLockBlobVersion GetLoadLockBlobVersion(const TKeyBlob& blob_id);
    TInfoLockBlobVersion GetLoadedBlobVersion(const TKeyBlob& blob_id);
    bool SetLoadedBlobVersion(const TKeyBlob& blob_id, TBlobVersion version);
    void SetAndSaveBlobVersion(const TKeyBlob& blob_id,
                               TBlobVersion version);

    /*
    void SetSeq_entry(const TKeyBlob& blob_id,
                      CLoadLockBlob& blob,
                      TChunkId chunk_id,
                      CRef<CSeq_entry> entry,
                      CTSE_SetObjectInfo* set_info = 0);
    */

    TInfoLockBlob GetLoadLockBlob(const TKeyBlob& blob_id);
    TInfoLockBlob GetLoadedBlob(const TKeyBlob& blob_id);
    // end of new lock manager support
    //////////////////////////////////////////////////////////////////

    virtual CGBDataLoader* GetLoaderPtr(void);

    static TKeyBlob_ids s_KeyBlobIds(const CSeq_id_Handle& id,
                                     const SAnnotSelector* sel);

    CTSE_LoadLock GetBlobLoadLock(const TKeyBlob& blob_id);

    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id) = 0;
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id) = 0;

    typedef vector<CBlob_id> TLoadedBlob_ids;
    virtual void GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                   TLoadedBlob_ids& blob_ids) const;

    bool SetNoBlob(const TKeyBlob& blob_id, TBlobState blob_state);
    void ReleaseNotLoadedBlobs(void);

    typedef CTSE_Lock               TTSE_Lock;
    typedef set<TTSE_Lock>          TTSE_LockSet;
    void SaveLocksTo(TTSE_LockSet& locks);

    void ReleaseLocks(void);

    virtual operator CInitMutexPool&(void) = 0;

    typedef int TConn;

    TLevel GetLevel() const { return m_Level; }
    void SetLevel(TLevel level) { m_Level = level; }

    const CSeq_id_Handle& GetRequestedId(void) const { return m_RequestedId; }
    void SetRequestedId(const CSeq_id_Handle& requested_id);

    void ClearRetryDelay(void) { m_RetryDelay = 0; }
    void AddRetryDelay(double delay) { m_RetryDelay += delay; }
    double GetRetryDelay(void) const { return m_RetryDelay; }

    // expiration support
    TExpirationTime GetStartTime(void) const { return m_StartTime; }
    TExpirationTime GetNewIdExpirationTime(void) const;
    virtual TExpirationTime GetIdExpirationTimeout(void) const;

    virtual bool GetAddWGSMasterDescr(void) const;

    CWriter* GetIdWriter(void) const;
    CWriter* GetBlobWriter(void) const;

    bool IsInProcessor(void) const { return m_InProcessor > 0; }

private:
    friend class CLoadLockBlob;
    friend class CLoadLockSetter;
    friend class CReaderAllocatedConnection;
    friend class CReaderRequestResultRecursion;

    void x_AddTSE_LoadLock(const CTSE_LoadLock& lock);

    CReadDispatcher& m_ReadDispatcher;
    TTSE_LockSet    m_TSE_LockSet;
    TLevel          m_Level;
    CSeq_id_Handle  m_RequestedId;
    int             m_RecursionLevel;
    int             m_InProcessor;
    double          m_RecursiveTime;
    CReaderAllocatedConnection* m_AllocatedConnection;
    double          m_RetryDelay;
    TExpirationTime m_StartTime;

private: // hide methods
    void* operator new(size_t size);
    void* operator new[](size_t size);
    CReaderRequestResult(const CReaderRequestResult&);
    CReaderRequestResult& operator=(const CReaderRequestResult&);
};


class CReaderRequestResultRecursion : public CStopWatch
{
public:
    explicit
    CReaderRequestResultRecursion(CReaderRequestResult& result,
                                  bool in_processor = false);
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
    bool m_InProcessor;
private: // to prevent copying
    CReaderRequestResultRecursion(const CReaderRequestResultRecursion&);
    void operator=(const CReaderRequestResultRecursion&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//GBLOADER_REQUEST_RESULT__HPP_INCLUDED
