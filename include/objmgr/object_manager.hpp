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

#include <objects/objmgr/data_loader.hpp>
#include <objects/objmgr/impl/priority.hpp>

#include <corelib/ncbiobj.hpp>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource;
class CBioseq;
class CSeq_id;
class CScope;
class CSeq_id_Mapper;


/////////////////////////////////////////////////////////////////////////////
// CObjectManager


class NCBI_XOBJMGR_EXPORT CObjectManager : public CObject
{
public:
    CObjectManager(void);
    virtual ~CObjectManager(void);

public:

// configuration functions
// this data is always available to scopes -
// by name - in case of data loader
// or by address - in case of Seq_entry

    // whether to put data loader or TSE to the default group or not
    enum EIsDefault {
        eDefault,
        eNonDefault
    };

    // Register existing data loader.
    // NOTE:  data loader must be created in the heap (ie using operator new).
    void RegisterDataLoader(CDataLoader& loader,
                            EIsDefault   is_default = eNonDefault);

    // Register data loader factory.
    // NOTE:  client has no control on when data loader is created or deleted.
    void RegisterDataLoader(CDataLoaderFactory& factory,
                            EIsDefault          is_default = eNonDefault);
    // RegisterDataLoader(*new CSimpleDataLoaderFactory<TDataLoader>(name), ...

    void RegisterDataLoader(TFACTORY_AUTOCREATE factory,
                            const string& loader_name,
                            EIsDefault   is_default = eNonDefault);


    // Revoke previously registered data loader.
    // Return FALSE if the loader is still in use (by some scope).
    // Throw an exception if the loader is not registered with this ObjMgr.
    bool RevokeDataLoader(CDataLoader& loader);
    bool RevokeDataLoader(const string& loader_name);

    // Register top-level seq_entry
    void RegisterTopLevelSeqEntry(CSeq_entry& top_entry);

    virtual CConstRef<CBioseq> GetBioseq(const CSeq_id& id);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

protected:

// functions for scopes
    void RegisterScope(CScope& scope);
    void RevokeScope  (CScope& scope);

    void AcquireDefaultDataSources(TDataSourceSet& sources,
                                   SDataSourceRec::TPriority priority);

    void AddDataLoader(
        TDataSourceSet& sources, const string& loader_name,
        SDataSourceRec::TPriority priority);
    void AddDataLoader(
        TDataSourceSet& sources, CDataLoader& loader,
        SDataSourceRec::TPriority priority);
    void AddTopLevelSeqEntry(
        TDataSourceSet& sources, CSeq_entry& top_entry,
        SDataSourceRec::TPriority priority);
    void RemoveTopLevelSeqEntry(
        TDataSourceSet& sources, CSeq_entry& top_entry);

    void ReleaseDataSources(TDataSourceSet& sources);


private:

// these are for Object Manager itself
// nobody else should use it
    void x_RegisterTSE(CSeq_entry& top_entry);
    bool x_RegisterLoader(CDataLoader& loader,EIsDefault   is_default = eNonDefault);
    CDataLoader* x_GetLoaderByName(const string& loader_name) const;
    
    void x_AddDataSource(
        TDataSourceSet& sources, CDataSource* source,
        SDataSourceRec::TPriority priority) const;
    void x_ReleaseDataSource(CDataSource* source);

private:

    set< CDataSource* > m_setDefaultSource;
    map< string, CDataLoader* > m_mapNameToLoader;
    map< CDataLoader* , CDataSource* > m_mapLoaderToSource;
    map< CSeq_entry* , CDataSource* > m_mapEntryToSource;
    set< CScope* > m_setScope;

    friend class CScope;
    friend class CDataSource; // To get id-mapper
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECT_MANAGER__HPP
