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

#include <objects/objmgr1/data_loader.hpp>

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

class CObjectManager : public CObject
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

protected:

// functions for scopes
    void RegisterScope(CScope& scope);
    void RevokeScope  (CScope& scope);

    void AcquireDefaultDataSources(set< CDataSource* >& sources);

    void AddDataLoader(
        set< CDataSource* >& sources, const string& loader_name);
    void AddDataLoader(
        set< CDataSource* >& sources, CDataLoader& loader);
/*
    void RemoveDataLoader(
        set< CDataSource* >& sources, CDataLoader& loader);
    void RemoveDataLoader(
        set< CDataSource* >& sources, const string& loader_name);
*/
    void AddTopLevelSeqEntry(
        set< CDataSource* >& sources, CSeq_entry& top_entry);
    void RemoveTopLevelSeqEntry(
        set< CDataSource* >& sources, CSeq_entry& top_entry);

    void ReleaseDataSources(set< CDataSource* >& sources);


private:

// these are for Object Manager itself
// nobody else should use it
    CDataLoader* x_GetLoaderByName(const string& loader_name) const;
    void x_AddDataSource(
        set< CDataSource* >& sources, CDataSource* source) const;
    void x_ReleaseDataSource(CDataSource* source);

private:

    set< CDataSource* > m_setDefaultSource;
    map< string, CDataLoader* > m_mapNameToLoader;
    map< CDataLoader* , CDataSource* > m_mapLoaderToSource;
    map< CSeq_entry* , CDataSource* > m_mapEntryToSource;
    set< CScope* > m_setScope;

    CRef<CSeq_id_Mapper> m_IdMapper;
    friend class CScope;
    friend class CDataSource; // To get id-mapper
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECT_MANAGER__HPP
