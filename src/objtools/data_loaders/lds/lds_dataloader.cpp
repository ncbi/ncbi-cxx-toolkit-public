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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  LDS dataloader. Implementations.
 *
 */


#include <ncbi_pch.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_util.hpp>

#include <objects/general/Object_id.hpp>

#include <bdb/bdb_util.hpp>
#include <objtools/lds/lds.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)


class CLDS_BlobId : public CObject
{
public:
    CLDS_BlobId(int rec_id)
    : m_RecId(rec_id)
    {}
    int GetRecId() const { return m_RecId; }
private:
    int    m_RecId;
};

/// @internal
///
struct SLDS_ObjectDisposition
{
    SLDS_ObjectDisposition(int object, int parent, int tse)
      : object_id(object), parent_id(parent), tse_id(tse)
    {}

    int     object_id;
    int     parent_id;
    int     tse_id;
};

/// Objects scanner
///
/// @internal
///
class CLDS_FindSeqIdFunc
{
public:
    typedef vector<SLDS_ObjectDisposition>  TDisposition;

public:
    CLDS_FindSeqIdFunc(SLDS_TablesCollection& db,
                       const CHandleRangeMap&  hrmap)
    : m_HrMap(hrmap),
      m_db(db)
    {}

    void operator()(SLDS_ObjectDB& dbf)
    {
        if (dbf.primary_seqid.IsNull())
            return;
        int object_id = dbf.object_id;
        int parent_id = dbf.parent_object_id;
        int tse_id    = dbf.TSE_object_id;


        string seq_id_str = (const char*)dbf.primary_seqid;
        if (seq_id_str.empty())
            return;
        {{
            CSeq_id seq_id_db(seq_id_str);
            if (seq_id_db.Which() == CSeq_id::e_not_set) {
                seq_id_db.SetLocal().SetStr(seq_id_str);
                if (seq_id_db.Which() == CSeq_id::e_not_set) {
                    return;
                }
            }

            ITERATE (CHandleRangeMap, it, m_HrMap) {
                CSeq_id_Handle seq_id_hnd = it->first;
                CConstRef<CSeq_id> seq_id = seq_id_hnd.GetSeqId();
                if (seq_id->Match(seq_id_db)) {
                    m_Disposition.push_back(
                        SLDS_ObjectDisposition(object_id, parent_id, tse_id));
                    return;
                }
            } // ITERATE
        }}

        // Primaty seq_id scan gave no results
        // Trying supplemental aliases
/*
        m_db.object_attr_db.object_attr_id = object_id;
        if (m_db.object_attr_db.Fetch() != eBDB_Ok) {
            return;
        }

        if (m_db.object_attr_db.seq_ids.IsNull() || 
            m_db.object_attr_db.seq_ids.IsEmpty()) {
            return;
        }
*/
        if (dbf.seq_ids.IsNull() || 
            dbf.seq_ids.IsEmpty()) {
            return;
        }
        string attr_seq_ids((const char*)dbf.seq_ids);
        vector<string> seq_id_arr;
        
        NStr::Tokenize(attr_seq_ids, " ", seq_id_arr, NStr::eMergeDelims);

        ITERATE (vector<string>, it, seq_id_arr) {
            CSeq_id seq_id_db(*it);

            if (seq_id_db.Which() == CSeq_id::e_not_set) {
                seq_id_db.SetLocal().SetStr(*it);
                if (seq_id_db.Which() == CSeq_id::e_not_set) {
                    continue;
                }
            }

            {{
            
            ITERATE (CHandleRangeMap, it2, m_HrMap) {
                CSeq_id_Handle seq_id_hnd = it2->first;
                CConstRef<CSeq_id> seq_id = seq_id_hnd.GetSeqId();

                if (seq_id->Match(seq_id_db)) {
                    m_Disposition.push_back(
                        SLDS_ObjectDisposition(object_id, parent_id, tse_id));
                    return;
                }
            } // ITERATE
            
            }}

        } // ITERATE
    }

    const TDisposition& GetResultDisposition() const 
    {
        return m_Disposition;
    }
private:
    const CHandleRangeMap&  m_HrMap;        ///< Range map of seq ids to search
    SLDS_TablesCollection&  m_db;          ///< The LDS database
    TDisposition            m_Disposition; ///< Search result (found objects)
};



CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TSimpleMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(void)
{
    return "LDS_dataloader";
}


CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CLDS_Database& lds_db,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TDbMaker maker(lds_db);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(CLDS_Database& lds_db)
{
    const string& alias = lds_db.GetAlias();
    if ( !alias.empty() ) {
        return "LDS_dataloader_" + alias;
    }
    return "LDS_dataloader_" + lds_db.GetDirName() + "_" + lds_db.GetDbName();
}


CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& db_path,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TPathMaker maker(db_path);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(const string& db_path)
{
    return "LDS_dataloader_" + db_path;
}


CLDS_DataLoader::CLDS_DataLoader(void)
    : CDataLoader(GetLoaderNameFromArgs()),
      m_LDS_db(0)
{
}


CLDS_DataLoader::CLDS_DataLoader(const string& dl_name)
    : CDataLoader(dl_name),
      m_LDS_db(0)
{
}



CLDS_DataLoader::CLDS_DataLoader(const string& dl_name,
                                 CLDS_Database& lds_db)
 : CDataLoader(dl_name),
   m_LDS_db(&lds_db),
   m_OwnDatabase(false)
{
}


CLDS_DataLoader::CLDS_DataLoader(const string& dl_name,
                                 const string& db_path)
 : CDataLoader(dl_name),
   m_LDS_db(new CLDS_Database(db_path, kEmptyStr)),
   m_OwnDatabase(true)
{
    try {
        m_LDS_db->Open();
    } 
    catch(...)
    {
        delete m_LDS_db;
        throw;
    }
}

CLDS_DataLoader::~CLDS_DataLoader()
{
    if (m_OwnDatabase)
        delete m_LDS_db;
}


void CLDS_DataLoader::SetDatabase(CLDS_Database& lds_db,
                                  const string&  dl_name)
{
    if (m_LDS_db && m_OwnDatabase)
        delete m_LDS_db;
    m_LDS_db = &lds_db;
    SetName(dl_name);
}


CDataLoader::TTSE_LockSet
CLDS_DataLoader::GetRecords(const CSeq_id_Handle& idh,
                            EChoice /* choice */)
{
    TTSE_LockSet locks;
    CHandleRangeMap hrmap;
    hrmap.AddRange(idh, CRange<TSeqPos>::GetWhole(), eNa_strand_unknown);
    CLDS_FindSeqIdFunc search_func(m_LDS_db->GetTables(), hrmap);
    
    SLDS_TablesCollection& db = m_LDS_db->GetTables();

    BDB_iterate_file(db.object_db, search_func);

    const CLDS_FindSeqIdFunc::TDisposition& disposition = 
                                        search_func.GetResultDisposition();

    CDataSource* data_source = GetDataSource();
    _ASSERT(data_source);

    CLDS_FindSeqIdFunc::TDisposition::const_iterator it;
    for (it = disposition.begin(); it != disposition.end(); ++it) {
        const SLDS_ObjectDisposition& obj_disp = *it;

        if (LDS_SetTest(m_LoadedObjects, obj_disp.object_id) ||
            LDS_SetTest(m_LoadedObjects, obj_disp.parent_id) ||
            LDS_SetTest(m_LoadedObjects, obj_disp.tse_id)
           ) {
            // Object or its parent has already been loaded. Ignore.
            continue;
        }

        int object_id = 
            obj_disp.tse_id ? obj_disp.tse_id : obj_disp.object_id;


        // check if we can extract seq-entry out of binary bioseq-set file
        //
        //   (this trick has been added by kuznets (Jan-12-2005) to read 
        //    molecules out of huge refseq files)
        {
            CLDS_Query query(db);
            CLDS_Query::SObjectDescr obj_descr = 
                query.GetObjectDescr(
                                 m_LDS_db->GetObjTypeMap(),
                                 obj_disp.tse_id, false/*do not trace to top*/);
            if ((obj_descr.is_object && obj_descr.id > 0)      &&
                (obj_descr.format == CFormatGuess::eBinaryASN) &&
                (obj_descr.type_str == "Bioseq-set")
               ) {
               obj_descr = 
                    query.GetTopSeqEntry(m_LDS_db->GetObjTypeMap(),
                                         obj_disp.object_id);
               object_id = obj_descr.id;
            }
        }


        CRef<CSeq_entry> seq_entry = 
            LDS_LoadTSE(db, m_LDS_db->GetObjTypeMap(), 
                        object_id, false/*dont trace to top*/);

        if (seq_entry) {
            CConstRef<CObject> blob_id(new CLDS_BlobId(object_id));
            CRef<CTSE_Info> tse_info(new CTSE_Info(*seq_entry,
                                                   CBioseq_Handle::fState_none,
                                                   blob_id));
            locks.insert(data_source->AddTSE(tse_info));
            m_LoadedObjects.insert(object_id);
        }
    }
    return locks;
}

void CLDS_DataLoader::DropTSE(const CTSE_Info& tse_info)
{
    const CConstRef<CObject>& cobj_ref = tse_info.GetBlobId();
    const CObject* obj_ptr = cobj_ref.GetPointerOrNull();
    _ASSERT(obj_ptr);

    const CLDS_BlobId* blob_id = 
        dynamic_cast<const CLDS_BlobId*>(obj_ptr);
    _ASSERT(blob_id);
    
    int object_id = blob_id->GetRecId();
    m_LoadedObjects.erase(object_id);
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_LDS(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_LDS);
}


const string kDataLoader_LDS_DriverName("lds");

class CLDS_DataLoaderCF : public CDataLoaderFactory
{
public:
    CLDS_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_LDS_DriverName) {}
    virtual ~CLDS_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CLDS_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CLDS_DataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
    const string& database_str =
        GetParam(GetDriverName(), params,
        kCFParam_LDS_Database, false, kEmptyStr);
    const string& db_path =
        GetParam(GetDriverName(), params,
        kCFParam_LDS_DbPath, false, kEmptyStr);
    if ( !database_str.empty() ) {
        // Use existing database
        CLDS_Database* db = dynamic_cast<CLDS_Database*>(
            static_cast<CDataLoader*>(
            const_cast<void*>(NStr::StringToPtr(database_str))));
        if ( db ) {
            return CLDS_DataLoader::RegisterInObjectManager(
                om,
                *db,
                GetIsDefault(params),
                GetPriority(params)).GetLoader();
        }
    }
    if ( !db_path.empty() ) {
        // Use db path
        return CLDS_DataLoader::RegisterInObjectManager(
            om,
            db_path,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CLDS_DataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_LDS(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CLDS_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_lds(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_LDS(info_list, method);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2005/02/02 19:49:55  grichenk
 * Fixed more warnings
 *
 * Revision 1.26  2005/01/26 16:25:21  grichenk
 * Added state flags to CBioseq_Handle.
 *
 * Revision 1.25  2005/01/13 17:52:16  kuznets
 * Tweak dataloader to read seq entries out of large binary ASN bioseq-sets
 *
 * Revision 1.24  2005/01/12 15:55:38  vasilche
 * Use correct constructor of CTSE_Info (detected by new bool operator).
 *
 * Revision 1.23  2005/01/11 19:30:29  kuznets
 * Fixed problem with loaded flag
 *
 * Revision 1.22  2004/12/22 20:42:53  grichenk
 * Added entry points registration funcitons
 *
 * Revision 1.21  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.20  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.19  2004/08/02 17:34:44  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.18  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.17  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.16  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.15  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/03/09 17:17:20  kuznets
 * Merge object attributes with objects
 *
 * Revision 1.13  2003/12/16 20:49:18  vasilche
 * Fixed compile errors - added missing includes and declarations.
 *
 * Revision 1.12  2003/12/16 17:55:22  kuznets
 * Added plugin entry point
 *
 * Revision 1.11  2003/11/28 13:41:10  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.10  2003/10/28 14:01:20  kuznets
 * Added constructor parameter name of the dataloader
 *
 * Revision 1.9  2003/10/08 19:40:55  kuznets
 * kEmptyStr instead database path as default alias
 *
 * Revision 1.8  2003/10/08 19:29:08  ucko
 * Adapt to new (multi-DB-capable) LDS API.
 *
 * Revision 1.7  2003/09/30 16:36:37  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.6  2003/08/19 14:21:24  kuznets
 * +name("LDS_dataloader") for the dataloader class
 *
 * Revision 1.5  2003/07/30 19:52:15  kuznets
 * Cleaned compilation warnings
 *
 * Revision 1.4  2003/07/30 19:46:54  kuznets
 * Implemented alias search mode
 *
 * Revision 1.3  2003/07/30 18:36:38  kuznets
 * Minor syntactic fix
 *
 * Revision 1.2  2003/06/18 18:49:01  kuznets
 * Implemented new constructor.
 *
 * Revision 1.1  2003/06/16 15:48:28  kuznets
 * Initial revision.
 *
 *
 * ===========================================================================
 */

