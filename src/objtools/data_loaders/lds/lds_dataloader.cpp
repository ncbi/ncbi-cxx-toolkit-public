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

#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_util.hpp>

#include <objects/general/Object_id.hpp>

#include <bdb/bdb_util.hpp>
#include <objtools/lds/lds.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>


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


class CLDS_FindSeqIdFunc
{
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
        int tse_id = dbf.TSE_object_id;

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
                const CSeq_id& seq_id = seq_id_hnd.GetSeqId();
                if (seq_id.Match(seq_id_db)) {
                    m_ids.insert(tse_id ? tse_id : object_id);
                    return;
                }
            } // ITERATE
        }}

        // Primaty seq_id scan gave no results
        // Trying supplemental aliases

        m_db.object_attr_db.object_attr_id = object_id;
        if (m_db.object_attr_db.Fetch() != eBDB_Ok) {
            return;
        }

        if (m_db.object_attr_db.seq_ids.IsNull() || 
            m_db.object_attr_db.seq_ids.IsEmpty()) {
            return;
        }

        string attr_seq_ids((const char*)m_db.object_attr_db.seq_ids);
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
                const CSeq_id& seq_id = seq_id_hnd.GetSeqId();

                if (seq_id.Match(seq_id_db)) {
                    m_ids.insert(tse_id ? tse_id : object_id);
                    return;
                }
            } // ITERATE
            
            }}

        } // ITERATE
    }

    CLDS_Set& GetIds() { return m_ids; }

private:
    const CHandleRangeMap&  m_HrMap;  // Range map of seq ids to search
    SLDS_TablesCollection&  m_db;     // The LDS database
    CLDS_Set                m_ids;    // TSE results, found
};


CLDS_DataLoader::CLDS_DataLoader(CLDS_Database& lds_db)
 : CDataLoader("LDS_dataloader"),
   m_LDS_db(lds_db),
   m_OwnDatabase(false)
{}

CLDS_DataLoader::CLDS_DataLoader(const string& db_path)
 : CDataLoader("LDS_dataloader"),
   m_LDS_db(*(new CLDS_Database(db_path))),
   m_OwnDatabase(true)
{
    try {
        m_LDS_db.Open();
    } 
    catch(...)
    {
        delete &m_LDS_db;
        throw;
    }
}

CLDS_DataLoader::~CLDS_DataLoader()
{
    if (m_OwnDatabase)
        delete &m_LDS_db;
}

bool CLDS_DataLoader::GetRecords(const CHandleRangeMap& hrmap,
                                 const EChoice choice)
{
    CLDS_FindSeqIdFunc search_func(m_LDS_db.GetTables(), hrmap);
    
    SLDS_TablesCollection& db = m_LDS_db.GetTables();

    BDB_iterate_file(db.object_db, search_func);

    const CLDS_Set& ids = search_func.GetIds();

    CDataSource* data_source = GetDataSource();
    _ASSERT(data_source);

    CLDS_Set::const_iterator it;
    for (it = ids.begin(); it != ids.end(); ++it) {
        int object_id = *it;

        if (LDS_SetTest(m_LoadedObjects, object_id)) {
            // Object has already been loaded. Ignore.
            continue;
        }

        CRef<CSeq_entry> seq_entry = 
            LDS_LoadTSE(db, m_LDS_db.GetObjTypeMap(), object_id);

        if (seq_entry) {
            CRef<CTSE_Info> tse_info = 
                data_source->AddTSE(*seq_entry,
                                    false,
                                    new CLDS_BlobId(object_id));
            m_LoadedObjects.insert(object_id);
        }
    }

    return true;
}

bool CLDS_DataLoader::DropTSE(const CTSE_Info& tse_info)
{
    const CConstRef<CObject>& cobj_ref = tse_info.GetBlobId();
    const CObject* obj_ptr = cobj_ref.GetPointerOrNull();
    _ASSERT(obj_ptr);

    const CLDS_BlobId* blob_id = 
        dynamic_cast<const CLDS_BlobId*>(obj_ptr);
    _ASSERT(blob_id);
    
    int object_id = blob_id->GetRecId();
    m_LoadedObjects.erase(object_id);
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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

