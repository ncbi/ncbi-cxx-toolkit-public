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
#include <objects/seq/seq_id_mapper.hpp>
#include <objmgr/impl/mutex_pool.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_lock.hpp>

#include <set>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CDataLoader;
class CSeqMap;
class CScope;
class CBioseq_Info;
class CSynonymsSet;
struct SSeq_id_ScopeInfo;

struct NCBI_XOBJMGR_EXPORT CDataSource_ScopeInfo : public CObject
{
    typedef CRef<CDataSource> TDataSourceLock;

    CDataSource_ScopeInfo(CDataSource& ds);
    ~CDataSource_ScopeInfo(void);

    typedef set<TTSE_Lock>                           TTSE_LockSet;

    const TTSE_LockSet& GetTSESet(void) const;

    CDataSource& GetDataSource(void);
    const CDataSource& GetDataSource(void) const;
    CDataLoader* GetDataLoader(void);

protected:
    friend class CScope_Impl;

    CFastMutex& GetMutex(void);

    void AddTSE(const TTSE_Lock& tse);
    void Reset(void);

private:
    CDataSource_ScopeInfo(const CDataSource_ScopeInfo&);
    const CDataSource_ScopeInfo& operator=(const CDataSource_ScopeInfo&);

    TDataSourceLock m_DataSource;
    TTSE_LockSet    m_TSE_LockSet;
    CFastMutex      m_Leaf_Mtx; // for m_TSE_LockSet
};


class NCBI_XOBJMGR_EXPORT CBioseq_ScopeInfo : public CObject
{
public:
    typedef pair<const CSeq_id_Handle, SSeq_id_ScopeInfo> TScopeInfo;

    CBioseq_ScopeInfo(TScopeInfo* scope_info); // no sequence
    CBioseq_ScopeInfo(TScopeInfo* scope_info,
                      const CConstRef<CBioseq_Info>& bioseq,
                      const TTSE_Lock& tse_lock);

    ~CBioseq_ScopeInfo(void);

    CScope& GetScope(void) const;

    bool HasBioseq(void) const;
    const CSeq_id_Handle& GetSeq_id_Handle(void) const;
    const CBioseq_Info& GetBioseq_Info(void) const;

    const TTSE_Lock& GetTSE_Lock(void) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CDataSource& GetDataSource(void) const;

     // check if the scope has not been reset
    void CheckScope(void) const;

private:
    friend class CScope_Impl;
    friend class CSeq_id_ScopeInfo;

    // owner scope's info
    TScopeInfo*              m_ScopeInfo;

    // bioseq object if any
    // if none -> no bioseq for this Seq_id, but there might be annotations
    // if buiseq exists, m_TSE_Lock holds lock for TSE containing bioseq
    CConstRef<CBioseq_Info>  m_Bioseq_Info;
    TTSE_Lock                m_TSE_Lock;

    // caches synonyms of bioseq if any
    // all synonyms share the same CBioseq_ScopeInfo object
    CInitMutex<CSynonymsSet> m_SynCache;

private:
    CBioseq_ScopeInfo(const CBioseq_ScopeInfo& info);
    const CBioseq_ScopeInfo& operator=(const CBioseq_ScopeInfo& info);
};


struct NCBI_XOBJMGR_EXPORT SSeq_id_ScopeInfo
{
    SSeq_id_ScopeInfo(CScope* scope);
    ~SSeq_id_ScopeInfo(void);

    typedef set<TTSE_Lock>                           TTSE_LockSet;
    typedef map<TTSE_Lock, TSeq_id_HandleSet>        TTSE_LockMap;
    typedef CObjectFor<TTSE_LockMap>                 TAnnotRefMap;
    typedef CInitMutex<TAnnotRefMap>                 TAnnotRefInfo;

    // owner scope
    CScope*                       m_Scope;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    CInitMutex<CBioseq_ScopeInfo> m_Bioseq_Info;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    TAnnotRefInfo                 m_AllAnnotRef_Info;
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
CFastMutex& CDataSource_ScopeInfo::GetMutex(void)
{
    return m_Leaf_Mtx;
}

/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
bool CBioseq_ScopeInfo::HasBioseq(void) const
{
    return m_Bioseq_Info;
}


inline
const CBioseq_Info& CBioseq_ScopeInfo::GetBioseq_Info(void) const
{
    return *m_Bioseq_Info;
}


inline
const CSeq_id_Handle& CBioseq_ScopeInfo::GetSeq_id_Handle(void) const
{
    return m_ScopeInfo->first;
}


inline
const TTSE_Lock& CBioseq_ScopeInfo::GetTSE_Lock(void) const
{
    return m_TSE_Lock;
}


inline
void CBioseq_ScopeInfo::CheckScope(void) const
{
    if ( !m_ScopeInfo ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "No scope associated with bioseq info");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
