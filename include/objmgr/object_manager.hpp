#ifndef OBJECT_MANAGER__HPP
#define OBJECT_MANAGER__HPP

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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*
* File Description:
*           Object manager manages data objects,
*           provides them to Scopes when needed
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.23  2004/07/12 15:24:59  grichenk
* Reverted changes related to CObjectManager singleton
*
* Revision 1.22  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.21  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.20  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.19  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.18  2003/09/30 16:21:59  vasilche
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
* Revision 1.17  2003/08/04 17:04:27  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.16  2003/06/30 18:41:05  vasilche
* Removed unused commented code.
*
* Revision 1.15  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.13  2003/04/09 16:04:29  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.12  2003/03/26 20:59:22  grichenk
* Removed commented-out code
*
* Revision 1.11  2003/03/11 14:15:49  grichenk
* +Data-source priority
*
* Revision 1.10  2003/01/29 22:03:43  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.9  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.8  2002/08/28 17:05:13  vasilche
* Remove virtual inheritance
*
* Revision 1.7  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.6  2002/05/28 18:01:10  gouriano
* DebugDump added
*
* Revision 1.5  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:02  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objmgr/data_loader_factory.hpp>

#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource;
class CDataLoader;
class CDataLoaderFactory;
class CSeq_entry;
class CBioseq;
class CSeq_id;
class CScope;
class CScope_Impl;


/////////////////////////////////////////////////////////////////////////////
// CObjectManager


class CSeq_id_Mapper;

class NCBI_XOBJMGR_EXPORT CObjectManager : public CObject
{
public:
    CObjectManager(void);
    virtual ~CObjectManager(void);

public:
    typedef CRef<CDataSource> TDataSourceLock;
    typedef int TPriority;

// configuration functions
// this data is always available to scopes -
// by name - in case of data loader
// or by address - in case of Seq_entry

    // whether to put data loader or TSE to the default group or not
    enum EIsDefault {
        eDefault,
        eNonDefault
    };
    enum EPriority {
        kPriority_NotSet = -1
    };

    // Register existing data loader.
    // NOTE:  data loader must be created in the heap (ie using operator new).
    void RegisterDataLoader(CDataLoader& loader,
                            EIsDefault   is_default = eNonDefault,
                            TPriority priority = kPriority_NotSet);

    // Register data loader factory.
    // NOTE:  client has no control on when data loader is created or deleted.
    void RegisterDataLoader(CDataLoaderFactory& factory,
                            EIsDefault          is_default = eNonDefault,
                            TPriority priority = kPriority_NotSet);
    // RegisterDataLoader(*new CSimpleDataLoaderFactory<TDataLoader>(name), ...

    void RegisterDataLoader(TFACTORY_AUTOCREATE factory,
                            const string& loader_name,
                            EIsDefault   is_default = eNonDefault,
                            TPriority priority = kPriority_NotSet);


    // Revoke previously registered data loader.
    // Return FALSE if the loader is still in use (by some scope).
    // Throw an exception if the loader is not registered with this ObjMgr.
    bool RevokeDataLoader(CDataLoader& loader);
    bool RevokeDataLoader(const string& loader_name);

    // Register top-level seq_entry
    //void RegisterTopLevelSeqEntry(CSeq_entry& top_entry);

    //CConstRef<CBioseq> GetBioseq(const CSeq_id& id);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

protected:

// functions for scopes
    void RegisterScope(CScope_Impl& scope);
    void RevokeScope  (CScope_Impl& scope);

    typedef set<TDataSourceLock> TDataSourcesLock;

    TDataSourceLock AcquireDataLoader(CDataLoader& loader);
    TDataSourceLock AcquireDataLoader(const string& loader_name);
    TDataSourceLock AcquireTopLevelSeqEntry(CSeq_entry& top_entry);
    void AcquireDefaultDataSources(TDataSourcesLock& sources);
    bool ReleaseDataSource(TDataSourceLock& data_source);

private:

// these are for Object Manager itself
// nobody else should use it
    TDataSourceLock x_RegisterTSE(CSeq_entry& top_entry);
    TDataSourceLock x_RegisterLoader(CDataLoader& loader,
                                     TPriority priority,
                                     EIsDefault   is_default = eNonDefault,
                                     bool         no_warning = false);
    CDataLoader* x_GetLoaderByName(const string& loader_name) const;
    TDataSourceLock x_FindDataSource(const CObject* key);
    TDataSourceLock x_RevokeDataLoader(CDataLoader* loader);
    
private:

    typedef set< TDataSourceLock >                  TSetDefaultSource;
    typedef map< string, CDataLoader* >             TMapNameToLoader;
    typedef map< const CObject* , TDataSourceLock > TMapToSource;
    typedef set< CScope_Impl* >                     TSetScope;

    TSetDefaultSource   m_setDefaultSource;
    TMapNameToLoader    m_mapNameToLoader;
    TMapToSource        m_mapToSource;
    TSetScope           m_setScope;
    
    typedef CMutex      TRWLock;
    typedef CMutexGuard TReadLockGuard;
    typedef CMutexGuard TWriteLockGuard;

    mutable TRWLock     m_OM_Lock;
    mutable TRWLock     m_OM_ScopeLock;

    // CSeq_id_Mapper lock to provide a single mapper while OM is running
    CRef<CSeq_id_Mapper> m_Seq_id_Mapper;

    friend class CScope_Impl;
    friend class CDataSource; // To get id-mapper
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECT_MANAGER__HPP
