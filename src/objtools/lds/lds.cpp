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
 * File Description:  LDS implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <bdb/bdb_cursor.hpp>

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_object.hpp>
#include <objtools/lds/lds_files.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_LDS

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



/* DEPRECATED */
CLDS_Database::CLDS_Database(const string& db_dir_name, 
                             const string& db_name,
                             const string& alias)
: m_LDS_DirName(db_dir_name)
{
    if (alias.empty()) {
        m_Alias = db_dir_name;
    }
    else {
        m_Alias = alias;
    }
}



CLDS_Database::CLDS_Database(const string& db_dir_name,
                             const string& alias)
: m_LDS_DirName(db_dir_name)
{
    if (alias.empty()) {
        m_Alias = db_dir_name;
    }
    else {
        m_Alias = alias;
    }
}


CLDS_Database::~CLDS_Database()
{
    LOG_POST_X(1, Info << "Closing LDS database: " <<  m_Alias);
}

void CLDS_Database::Create()
{
    string fname;
    LOG_POST_X(2, Info << "Creating LDS database: " <<  m_Alias);

    {{
        m_LDS_DirName = CDirEntry::AddTrailingPathSeparator(m_LDS_DirName);

        // Conditionally add LDS subdirectory name 
        // TODO: make name check more robust
        if (m_LDS_DirName.find("LDS") == string::npos) {
            m_LDS_DirName = CDirEntry::AddTrailingPathSeparator(m_LDS_DirName);
            m_LDS_DirName += "LDS";
            m_LDS_DirName = CDirEntry::AddTrailingPathSeparator(m_LDS_DirName);
        }

        CDir  lds_dir(m_LDS_DirName);

        if (!lds_dir.Exists()) {
            if (!lds_dir.Create()) {
                LDS_THROW(eCannotCreateDir, 
                          "Cannot create directory:"+m_LDS_DirName);
            }
        }

    }}

    m_db.reset(new SLDS_TablesCollection);

    LOG_POST_X(3, Info << "Creating LDS table: " << "file");

    fname = m_LDS_DirName + "lds_file.db";
    const char* c = fname.c_str(); 
    m_db->file_db.Open(c,
                      "file",
                      CBDB_RawFile::eCreate);

    LOG_POST_X(4, Info << "Creating LDS table: " << "objecttype");

    fname = m_LDS_DirName + "lds_objecttype.db"; 
    m_db->object_type_db.Open(fname.c_str(),
                             "objecttype",
                             CBDB_RawFile::eCreate);
    LOG_POST_X(5, Info << "Creating LDS table: " << "object");

    fname = m_LDS_DirName + "lds_object.db"; 
    m_db->object_db.SetCacheSize(3 * (1024 * 1024));
    m_db->object_db.Open(fname.c_str(),
                    "object",
                    CBDB_RawFile::eCreate);
/*
    LOG_POST_X(6, Info << "Creating LDS table: " << "objectattr");
    fname = m_LDS_DirName + "lds_objectattr.db"; 
    m_db.object_attr_db.Open(fname.c_str(),
                             "objectattr",
                             CBDB_RawFile::eCreate);
*/
    LOG_POST_X(7, Info << "Creating LDS table: " << "annotation");
    fname = m_LDS_DirName + "lds_annotation.db"; 
    m_db->annot_db.Open(fname.c_str(),
                       "annotation",
                       CBDB_RawFile::eCreate);

    LOG_POST_X(8, Info << "Creating LDS table: " << "annot2obj");
    fname = m_LDS_DirName + "lds_annot2obj.db"; 
    m_db->annot2obj_db.Open(fname.c_str(),
                           "annot2obj",
                           CBDB_RawFile::eCreate);

    LOG_POST_X(9, Info << "Creating LDS table: " << "seq_id_list");
    fname = m_LDS_DirName + "lds_seq_id_list.db"; 
    m_db->seq_id_list.Open(fname.c_str(),
                          "seq_id_list",
                          CBDB_RawFile::eCreate);

    LOG_POST_X(10, Info << "Creating LDS index: " << "obj_seqid_txt.idx");
    fname = m_LDS_DirName + "obj_seqid_txt.idx"; 
    m_db->obj_seqid_txt_idx.Open(fname.c_str(), 
                                CBDB_RawFile::eCreate);

    LOG_POST_X(11, Info << "Creating LDS index: " << "obj_seqid_int.idx");
    fname = m_LDS_DirName + "obj_seqid_int.idx"; 
    m_db->obj_seqid_int_idx.Open(fname.c_str(), 
                                CBDB_RawFile::eCreate);

    LOG_POST_X(12, Info << "Creating LDS index: " << "file_filename.idx");
    fname = m_LDS_DirName + "file_filename.idx"; 
    m_db->file_filename_idx.Open(fname.c_str(),
                                 CBDB_RawFile::eCreate);

}


void CLDS_Database::Sync()
{
    m_db->annot2obj_db.Sync();
    m_db->annot_db.Sync();
    m_db->file_db.Sync();
//    m_db.object_attr_db.Sync();
    m_db->object_db.Sync();
    m_db->object_type_db.Sync();
    m_db->seq_id_list.Sync();  

    m_db->obj_seqid_txt_idx.Sync();
    m_db->obj_seqid_int_idx.Sync();
    m_db->file_filename_idx.Sync();
}

void CLDS_Database::ReOpen()
{
    m_db.reset(0);
    Open(m_OpenMode);
}


void CLDS_Database::Open(EOpenMode omode)
{
    string fname;

    m_OpenMode = omode;

    CBDB_RawFile::EOpenMode om = CBDB_RawFile::eReadOnly;
    switch (omode) {
    case eReadWrite:
        om = CBDB_RawFile::eReadWrite;
        break;
    case eReadOnly:
        om = CBDB_RawFile::eReadOnly;
        break;
    default:
        _ASSERT(0);
    } // switch

    m_db.reset(new SLDS_TablesCollection);

    m_LDS_DirName = CDirEntry::AddTrailingPathSeparator(m_LDS_DirName);

    if (m_LDS_DirName.find("LDS") == string::npos) {
        m_LDS_DirName += "LDS";
        m_LDS_DirName = CDirEntry::AddTrailingPathSeparator(m_LDS_DirName);
    }

    fname = m_LDS_DirName + "lds_file.db"; 
    m_db->file_db.Open(fname.c_str(),
                      "file",
                      om);

    fname = m_LDS_DirName + "lds_objecttype.db"; 
    m_db->object_type_db.Open(fname.c_str(),
                             "objecttype",
                             om);
    LoadTypeMap();

    fname = m_LDS_DirName + "lds_object.db"; 
    m_db->object_db.SetCacheSize(3 * (1024 * 1024));
    m_db->object_db.Open(fname.c_str(),
                         "object",
                         om);
/*
    fname = m_LDS_DirName + "lds_objectattr.db"; 
    m_db.object_attr_db.Open(fname.c_str(),
                            "objectattr",
                            om);
*/
    fname = m_LDS_DirName + "lds_annotation.db"; 
    m_db->annot_db.Open(fname.c_str(),
                       "annotation",
                       om);

    fname = m_LDS_DirName + "lds_annot2obj.db"; 
    m_db->annot2obj_db.Open(fname.c_str(),
                           "annot2obj",
                           om);

    fname = m_LDS_DirName + "lds_seq_id_list.db"; 
    m_db->seq_id_list.Open(fname.c_str(),
                          "seq_id_list",
                           om);

    fname = m_LDS_DirName + "obj_seqid_txt.idx"; 
    m_db->obj_seqid_txt_idx.Open(fname.c_str(),
                                om);

    fname = m_LDS_DirName + "obj_seqid_int.idx"; 
    m_db->obj_seqid_int_idx.Open(fname.c_str(),
                                 om);

    fname = m_LDS_DirName + "file_filename.idx"; 
    m_db->file_filename_idx.Open(fname.c_str(),
                                 om);

}

void CLDS_Database::LoadTypeMap()
{
    if (!m_ObjTypeMap.empty()) {
        m_ObjTypeMap.erase(m_ObjTypeMap.begin(), m_ObjTypeMap.end());
    }
    CBDB_FileCursor cur(m_db->object_type_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        m_ObjTypeMap.insert(pair<string, int>(string(m_db->object_type_db.type_name), 
                                              m_db->object_type_db.object_type)
                           );
    }
}

/* DEPRECATED */
CRef<CLDS_DataLoader> CLDS_Database::GetLoader()
{ 
    return m_Loader; 
}
/* DEPRECATED */
void CLDS_Database::SetLoader( CRef<CLDS_DataLoader> aLoader )
{
    m_Loader = aLoader;
}


CLDS_DatabaseHolder::CLDS_DatabaseHolder(CLDS_Database* db) 
{
    if (db) {
        m_DataBases.push_back(db);
    }
}


CLDS_DatabaseHolder::~CLDS_DatabaseHolder() 
{ 
    ITERATE(vector<CLDS_Database*>, it, m_DataBases) {
        CLDS_Database* db = *it;
        delete db;
    }   
}

CLDS_Database* CLDS_DatabaseHolder::GetDatabase(const string& alias)
{
    ITERATE(vector<CLDS_Database*>, it, m_DataBases) {
        CLDS_Database* db = *it;
        const string& db_alias = db->GetAlias();
        if (db_alias == alias) {
            return db;
        }
    }
    return 0;
    
}

void CLDS_DatabaseHolder::EnumerateAliases(vector<string>* aliases)
{
    ITERATE(vector<CLDS_Database*>, it, m_DataBases) {
        CLDS_Database* db = *it;
        const string& db_alias = db->GetAlias();
        aliases->push_back(db_alias);
    }
}

void CLDS_DatabaseHolder::RemoveDatabase(CLDS_Database* db)
{
    NON_CONST_ITERATE(vector<CLDS_Database*>, it, m_DataBases) {
        if( *it == db ){
            m_DataBases.erase( it );
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
