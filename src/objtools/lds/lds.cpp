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

#include <bdb/bdb_cursor.hpp>

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_coreobjreader.hpp>
#include <objtools/lds/lds_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CLDS_Database::Create()
{
    LOG_POST(Info << "Creating LDS database: " << m_LDS_DbName);
    LOG_POST(Info << "Creating LDS table: " << "file");

    m_db.file_db.Open(m_LDS_DbName.c_str(),
                      "file",
                      CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "objecttype");

    m_db.object_type_db.Open(m_LDS_DbName.c_str(),
                             "objecttype",
                             CBDB_RawFile::eCreate);

    //
    // Upload the object type DB
    //
    LOG_POST(Info << "Uploading " << "objecttype");

    CLDS_CoreObjectsReader  core_reader;

    const CLDS_CoreObjectsReader::TCandidates& cand 
                                = core_reader.GetCandidates();
    
    int id = 1;
    m_db.object_type_db.object_type = id;
    m_db.object_type_db.type_name = "FastaEntry";
    m_db.object_type_db.Insert();

    LOG_POST(Info << "  " << "FastaEntry");

    m_ObjTypeMap.insert(pair<string, int>("FastaEntry", id));
    ++id;
    ITERATE(CLDS_CoreObjectsReader::TCandidates, it, cand) {
        string type_name = it->GetTypeInfo()->GetName();

        m_db.object_type_db.object_type = id;
        m_db.object_type_db.type_name = type_name;
        m_db.object_type_db.Insert();

        LOG_POST(Info << "  " << type_name);

        m_ObjTypeMap.insert(pair<string, int>(type_name, id));
        ++id;
    }

    LOG_POST(Info << "Creating LDS table: " << "object");
    m_db.object_db.Open(m_LDS_DbName.c_str(),
                    "object",
                    CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "objectattr");
    m_db.object_db.Open(m_LDS_DbName.c_str(),
                        "objectattr",
                        CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "annotation");
    m_db.annot_db.Open(m_LDS_DbName.c_str(),
                       "annotation",
                       CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "annot2obj");
    m_db.annot2obj_db.Open(m_LDS_DbName.c_str(),
                           "annot2obj",
                           CBDB_RawFile::eCreate);
}


void CLDS_Database::Open()
{
    m_db.file_db.Open(m_LDS_DbName.c_str(),
                      "file",
                      CBDB_RawFile::eReadWrite);

    m_db.object_type_db.Open(m_LDS_DbName.c_str(),
                             "objecttype",
                             CBDB_RawFile::eReadWrite);
    x_LoadTypeMap();

    m_db.object_db.Open(m_LDS_DbName.c_str(),
                        "object",
                        CBDB_RawFile::eReadWrite);

    m_db.object_attr_db.Open(m_LDS_DbName.c_str(),
                            "objectattr",
                            CBDB_RawFile::eReadWrite);

    m_db.annot_db.Open(m_LDS_DbName.c_str(),
                       "annotation",
                       CBDB_RawFile::eReadWrite);

    m_db.annot2obj_db.Open(m_LDS_DbName.c_str(),
                           "annot2obj",
                           CBDB_RawFile::eReadWrite);
}

void CLDS_Database::SyncWithDir(const string& dir_name)
{
    CLDS_Set files_deleted;
    CLDS_Set files_updated;

    CLDS_File fl(m_db.file_db);
    fl.SyncWithDir(dir_name, &files_deleted, &files_updated);


    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;

    CLDS_Object obj(m_db, m_ObjTypeMap);
    obj.DeleteCascadeFiles(files_deleted, &objects_deleted, &annotations_deleted);
    obj.UpdateCascadeFiles(files_updated);
}

void CLDS_Database::x_LoadTypeMap()
{
    CBDB_FileCursor cur(m_db.object_type_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        m_ObjTypeMap.insert(pair<string, int>(string(m_db.object_type_db.type_name), 
                                              m_db.object_type_db.object_type)
                           );
    }    
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/06/03 14:13:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */



