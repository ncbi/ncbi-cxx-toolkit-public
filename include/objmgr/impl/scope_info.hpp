#ifndef SCOPE_INFO__HPP
#define SCOPE_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*     Structures used by CScope
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/impl/mutex_pool.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objmgr/impl/tse_scope_lock.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <set>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CObjectManager;
class CDataSource;
class CDataLoader;
class CSeqMap;
class CScope;
class CScope_Impl;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_Info;
class CSynonymsSet;
class CDataSource_ScopeInfo;
class CTSE_ScopeInfo;
class CBioseq_ScopeInfo;
class CTSE_Handle;
struct SSeqMatch_Scope;
struct SSeqMatch_DS;
struct SSeq_id_ScopeInfo;
class CSeq_entry;
class CSeq_annot;
class CBioseq;

template<typename Key, typename Value>
class CDeleteQueue
{
public:
    typedef Key key_type;
    typedef Value value_type;

    CDeleteQueue(size_t max_size = 0)
        : m_MaxSize(max_size)
        {
        }

    bool Contains(const key_type& key) const
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            return m_Index.find(key) != m_Index.end();
        }

    value_type Get(const key_type& key)
        {
            value_type ret;

            _ASSERT(m_Queue.size() == m_Index.size());
            TIndexIter iter = m_Index.find(key);
            if ( iter != m_Index.end() ) {
                ret = iter->second->second;
                m_Queue.erase(iter->second);
                m_Index.erase(iter);
                _ASSERT(m_Queue.size() == m_Index.size());
            }

            return ret;
        }
    void Put(const key_type& key, const value_type& value)
        {
            _ASSERT(m_Queue.size() == m_Index.size());
            _ASSERT(m_Index.find(key) == m_Index.end());

            TQueueIter queue_iter =
                m_Queue.insert(m_Queue.end(), TQueueValue(key, value));
                               
            typedef typename TIndex::value_type insert_type;
            
            _VERIFY(m_Index.insert(TIndexValue(key, queue_iter)).second);

            _ASSERT(m_Queue.size() == m_Index.size());

            while ( m_Index.size() > m_MaxSize ) {
                _VERIFY(m_Index.erase(m_Queue.front().first) == 1);
                m_Queue.pop_front();
                _ASSERT(m_Queue.size() == m_Index.size());
            }
        }

    void Clear(void)
        {
            m_Queue.clear();
            m_Index.clear();
        }

private:
    typedef pair<key_type, value_type> TQueueValue;
    typedef list<TQueueValue> TQueue;
    typedef typename TQueue::iterator TQueueIter;

    typedef map<key_type, TQueueIter> TIndex;
    typedef typename TIndex::value_type TIndexValue;
    typedef typename TIndex::iterator TIndexIter;

    size_t m_MaxSize;
    TQueue m_Queue;
    TIndex m_Index;
};


class NCBI_XOBJMGR_EXPORT CDataSource_ScopeInfo : public CObject
{
public:
    typedef CRef<CDataSource>                           TDataSourceLock;
    typedef CRef<CTSE_ScopeInfo>                        TTSE_ScopeInfo;
    struct STSE_Key
    {
        typedef CConstRef<CObject> TBlobId;

        STSE_Key(const CTSE_Info& tse, bool can_be_unloaded);
        
        CDataLoader*    m_Loader;
        TBlobId         m_BlobId;

        bool operator<(const STSE_Key& tse2) const;
    };
    typedef map<STSE_Key, TTSE_ScopeInfo>               TTSE_InfoMap;
    typedef CTSE_LockSet                                TTSE_LockSet;
    typedef multimap<CSeq_id_Handle, TTSE_ScopeInfo>    TTSE_BySeqId;
    typedef CDeleteQueue<const CTSE_ScopeInfo*,
                         CTSE_ScopeInternalLock>        TTSE_UnlockQueue;

    CDataSource_ScopeInfo(CScope_Impl& scope, CDataSource& ds);
    ~CDataSource_ScopeInfo(void);

    void DetachFromOM(CObjectManager& om);
    CScope_Impl& GetScopeImpl(void) const;

    void Reset(void);

    typedef CMutex TTSE_InfoMapMutex;
    const TTSE_InfoMap& GetTSE_InfoMap(void) const;
    TTSE_InfoMapMutex& GetTSE_InfoMapMutex(void) const;

    typedef CMutex TTSE_LockSetMutex;
    const TTSE_LockSet& GetTSE_LockSet(void) const;
    TTSE_LockSetMutex& GetTSE_LockSetMutex(void) const;
    void UpdateTSELock(CTSE_ScopeInfo& tse, const CTSE_Lock& lock);
    void ReleaseTSELock(CTSE_ScopeInfo& tse); // into queue
    void ForgetTSELock(CTSE_ScopeInfo& tse); // completely
    bool UnlockTSE(CTSE_ScopeInfo& tse);
    bool TSEIsInQueue(const CTSE_ScopeInfo& tse) const;

    CDataSource& GetDataSource(void);
    const CDataSource& GetDataSource(void) const;
    CDataLoader* GetDataLoader(void);

    typedef CTSE_ScopeUserLock                          TTSE_Lock;
    typedef pair<CConstRef<CSeq_entry_Info>, TTSE_Lock> TSeq_entry_Lock;
    typedef pair<CConstRef<CSeq_annot_Info>, TTSE_Lock> TSeq_annot_Lock;
    typedef pair<CConstRef<CBioseq_Info>, TTSE_Lock>    TBioseq_Lock;

    TTSE_Lock GetTSE_Lock(const CTSE_Lock& tse);
    TTSE_Lock FindTSE_Lock(const CSeq_entry& tse);
    TSeq_entry_Lock FindSeq_entry_Lock(const CSeq_entry& entry);
    TSeq_annot_Lock FindSeq_annot_Lock(const CSeq_annot& annot);
    TBioseq_Lock FindBioseq_Lock(const CBioseq& bioseq);

    SSeqMatch_Scope BestResolve(const CSeq_id_Handle& idh, int get_flag);

protected:
    SSeqMatch_Scope x_GetSeqMatch(const CSeq_id_Handle& idh);
    SSeqMatch_Scope x_FindBestTSE(const CSeq_id_Handle& idh);
    void x_SetMatch(SSeqMatch_Scope& match,
                    CTSE_ScopeInfo& tse,
                    const CSeq_id_Handle& idh) const;
    void x_SetMatch(SSeqMatch_Scope& match,
                    const SSeqMatch_DS& ds_match);

    void x_IndexTSE(CTSE_ScopeInfo& tse);
    bool x_IsBetter(const CSeq_id_Handle& idh,
                    const CTSE_ScopeInfo& tse1,
                    const CTSE_ScopeInfo& tse2);

private: // members
    CScope_Impl*                m_Scope;
    TDataSourceLock             m_DataSource;
    bool                        m_CanBeUnloaded;
    int                         m_NextTSEIndex;
    TTSE_InfoMap                m_TSE_InfoMap;
    mutable TTSE_InfoMapMutex   m_TSE_InfoMapMutex;
    TTSE_BySeqId                m_TSE_BySeqId;
    TTSE_LockSet                m_TSE_LockSet;
    TTSE_UnlockQueue            m_TSE_UnlockQueue;
    mutable TTSE_LockSetMutex   m_TSE_LockSetMutex;

private: // to prevent copying
    CDataSource_ScopeInfo(const CDataSource_ScopeInfo&);
    const CDataSource_ScopeInfo& operator=(const CDataSource_ScopeInfo&);
};


class NCBI_XOBJMGR_EXPORT CTSE_ScopeInfo : public CObject
{
public:
    typedef CConstRef<CObject>                            TBlobId;
    typedef map<CSeq_id_Handle, CRef<CBioseq_ScopeInfo> > TBioseqs;
    typedef vector<CSeq_id_Handle>                        TBioseqsIds;
    typedef pair<int, int>                                TBlobOrder;
    typedef set<CTSE_ScopeInternalLock>                   TUsedTSE_LockSet;

    CTSE_ScopeInfo(CDataSource_ScopeInfo& ds_info,
                   const CTSE_Lock& tse_lock,
                   int load_index,
                   bool can_be_unloaded);
    ~CTSE_ScopeInfo(void);

    CScope_Impl& GetScopeImpl(void) const;
    CDataSource_ScopeInfo& GetDSInfo(void) const;

    CRef<CBioseq_ScopeInfo> GetBioseqInfo(const SSeqMatch_Scope& match);
    bool HasResolvedBioseq(const CSeq_id_Handle& id) const;

    bool CanBeUnloaded(void) const;
    // True if the TSE is referenced
    bool IsLocked(void) const;
    // True if the TSE is referenced by more than one handle
    bool LockedMoreThanOnce(void) const;

    bool ContainsBioseq(const CSeq_id_Handle& id) const;
    bool ContainsMatchingBioseq(const CSeq_id_Handle& id) const;

    void ClearCacheOnRemoveData(const CBioseq_Info& bioseq);

    bool x_SameTSE(const CTSE_Info& tse) const;

    int GetLoadIndex(void) const;
    TBlobOrder GetBlobOrder(void) const;
    const TBioseqsIds& GetBioseqsIds(void) const;

    bool AddUsedTSE(const CTSE_ScopeUserLock& lock) const;

protected:
    void x_LockTSE(void);
    void x_LockTSE(const CTSE_Lock& tse_lock);
    void x_RelockTSE(void);
    void x_UserUnlockTSE(void);
    void x_InternalUnlockTSE(void);
    void x_ReleaseTSE(void);

    void x_ForgetLocks(void);

    // Number of internal locks, not related to handles
    int x_GetDSLocksCount(void) const;

private: // members
    friend class CScope_Impl;
    friend class CDataSource_ScopeInfo;
    friend class CTSE_ScopeInternalLock;
    friend class CTSE_ScopeUserLock;

    CDataSource_ScopeInfo*      m_DS_Info;
    int                         m_LoadIndex;
    struct SUnloadedInfo {
        SUnloadedInfo(const CTSE_Lock& lock);
        CTSE_Lock LockTSE(void);

        CRef<CDataLoader>       m_Loader;
        TBlobId                 m_BlobId;
        TBlobOrder              m_BlobOrder;
        TBioseqsIds             m_BioseqsIds;
    };

    AutoPtr<SUnloadedInfo>      m_UnloadedInfo;
    TBioseqs                    m_Bioseqs;
    // TSE locking support
    mutable CAtomicCounter      m_TSE_LockCounter;
    mutable CTSE_Lock           m_TSE_Lock;
    // Used by TSE support
    mutable const CTSE_ScopeInfo*   m_UsedByTSE;
    mutable TUsedTSE_LockSet        m_UsedTSE_Set;

private: // to prevent copying
    CTSE_ScopeInfo(const CTSE_ScopeInfo&);
    CTSE_ScopeInfo& operator=(const CTSE_ScopeInfo&);
};


class NCBI_XOBJMGR_EXPORT CBioseq_ScopeInfo : public CObject
{
public:
    typedef CRef<CTSE_ScopeInfo>                        TTSE_ScopeInfo;
    typedef set<CSeq_id_Handle>                         TSeq_idSet;
    typedef map<TTSE_ScopeInfo, TSeq_idSet>             TTSE_MatchSet;
    typedef CObjectFor<TTSE_MatchSet>                   TTSE_MatchSetObject;
    typedef CInitMutex<TTSE_MatchSetObject>             TAnnotRefInfo;
    typedef vector<CSeq_id_Handle>                      TIds;
    typedef int                                         TBlobStateFlags;

    CBioseq_ScopeInfo(CScope_Impl& scope); // no sequence
    CBioseq_ScopeInfo(const SSeqMatch_Scope& match);
    ~CBioseq_ScopeInfo(void);

    CScope_Impl& GetScopeImpl(void) const;
    const CTSE_ScopeInfo& GetTSE_ScopeInfo(void) const;
    const CSeq_id_Handle& GetLookupId(void) const;

    bool HasBioseq(void) const;
    CConstRef<CBioseq_Info> GetBioseqInfo(const CTSE_Info& tse) const;

    const TIds& GetIds(void) const;

    string IdString(void) const;

    TBlobStateFlags GetBlobState(void) const
    {
        return m_BlobState;
    }

protected: // protected object manager interface
    friend class CScope_Impl;
    friend class CTSE_ScopeInfo;
    friend class CSeq_id_ScopeInfo;

    struct SUnloadedInfo
    {
        CSeq_id_Handle  m_LookupId;
        TIds            m_Ids;
    };

private: // members
    // TSE info where bioseq is located
    CScope_Impl*                    m_ScopeInfo;
    const CTSE_ScopeInfo*           m_TSE_ScopeInfo;
    AutoPtr<SUnloadedInfo>          m_UnloadedInfo;
    
    // cache CBioseq_Info pointer
    mutable CConstRef<CBioseq_Info> m_Bioseq;

    // cache synonyms of bioseq if any
    // all synonyms share the same CBioseq_ScopeInfo object
    CInitMutex<CSynonymsSet>        m_SynCache;

    // cache and lock TSEs with external annotations on this Bioseq
    CInitMutex<TTSE_MatchSetObject> m_BioseqAnnotRef_Info;

    TBlobStateFlags                m_BlobState;

private: // to prevent copying
    CBioseq_ScopeInfo(const CBioseq_ScopeInfo& info);
    const CBioseq_ScopeInfo& operator=(const CBioseq_ScopeInfo& info);
};


struct NCBI_XOBJMGR_EXPORT SSeq_id_ScopeInfo
{
    SSeq_id_ScopeInfo(void);
    ~SSeq_id_ScopeInfo(void);

    typedef CBioseq_ScopeInfo::TTSE_MatchSetObject TTSE_MatchSetObject;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    CInitMutex<CBioseq_ScopeInfo>   m_Bioseq_Info;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    CInitMutex<TTSE_MatchSetObject> m_AllAnnotRef_Info;
};

/////////////////////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
CDataSource& CDataSource_ScopeInfo::GetDataSource(void)
{
    return *m_DataSource;
}


inline
const CDataSource& CDataSource_ScopeInfo::GetDataSource(void) const
{
    return *m_DataSource;
}


inline
CDataSource_ScopeInfo::TTSE_InfoMapMutex&
CDataSource_ScopeInfo::GetTSE_InfoMapMutex(void) const
{
    return m_TSE_InfoMapMutex;
}

inline
CDataSource_ScopeInfo::TTSE_LockSetMutex&
CDataSource_ScopeInfo::GetTSE_LockSetMutex(void) const
{
    return m_TSE_LockSetMutex;
}


/////////////////////////////////////////////////////////////////////////////
// CTSE_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
int CTSE_ScopeInfo::GetLoadIndex(void) const
{
    return m_LoadIndex;
}


inline
bool CTSE_ScopeInfo::CanBeUnloaded(void) const
{
    return m_UnloadedInfo;
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
bool CBioseq_ScopeInfo::HasBioseq(void) const
{
    return m_Bioseq || m_UnloadedInfo;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/03/14 18:17:14  grichenk
* Added CScope::RemoveFromHistory(), CScope::RemoveTopLevelSeqEntry() and
* CScope::RemoveDataLoader(). Added requested seq-id information to
* CTSE_Info.
*
* Revision 1.16  2005/01/26 16:25:21  grichenk
* Added state flags to CBioseq_Handle.
*
* Revision 1.15  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.14  2004/12/28 18:39:46  vasilche
* Added lost scope lock in CTSE_Handle.
*
* Revision 1.13  2004/12/22 15:56:26  vasilche
* Used deep copying of CPriorityTree in AddScope().
* Implemented auto-release of unused TSEs in scope.
* Introduced CTSE_Handle.
* Fixed annotation collection.
* Removed too long cvs log.
*
* Revision 1.12  2004/10/25 16:53:31  vasilche
* Added suppord for orphan annotations.
*
* Revision 1.11  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.10  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.9  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.8  2004/01/27 17:10:12  ucko
* +tse_info.hpp due to use of CConstRef<CTSE_Info>
*
* Revision 1.7  2003/11/17 16:03:13  grichenk
* Throw exception in CBioseq_Handle if the parent scope has been reset
*
* Revision 1.6  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.5  2003/09/30 16:22:01  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.4  2003/06/19 19:48:16  vasilche
* Removed unnecessary copy constructor of SSeq_id_ScopeInfo.
*
* Revision 1.3  2003/06/19 19:31:00  vasilche
* Added missing CBioseq_ScopeInfo destructor for MSVC.
*
* Revision 1.2  2003/06/19 19:08:31  vasilche
* Added include to make MSVC happy.
*
* Revision 1.1  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/

#endif  // SCOPE_INFO__HPP
