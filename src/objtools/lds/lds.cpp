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
#include <objtools/lds/admin/lds_coreobjreader.hpp>
#include <objtools/lds/admin/lds_object.hpp>
#include <objtools/lds/admin/lds_files.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CLDS_Database::CLDS_Database(const string& db_dir_name, const string& db_name)
: m_LDS_DirName(db_dir_name),
  m_LDS_DbName(db_name)
{}

CLDS_Database::CLDS_Database(const string& db_dir_name)
: m_LDS_DirName(db_dir_name)
{
    m_LDS_DbName = "lds.db";
}


CLDS_Database::~CLDS_Database()
{
    LOG_POST(Info << "Closing LDS database: " << m_LDS_DbName);
}

void CLDS_Database::Create()
{
    string fname;
    LOG_POST(Info << "Creating LDS database: " << m_LDS_DbName);
    LOG_POST(Info << "Creating LDS table: " << "file");

    fname = m_LDS_DirName + "//lds_file.db"; 
    m_db.file_db.Open(fname.c_str(),
                      "file",
                      CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "objecttype");

    fname = m_LDS_DirName + "//lds_objecttype.db"; 
    m_db.object_type_db.Open(fname.c_str(),
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
        string type_name = it->type_info.GetTypeInfo()->GetName();

        m_db.object_type_db.object_type = id;
        m_db.object_type_db.type_name = type_name;
        m_db.object_type_db.Insert();

        LOG_POST(Info << "  " << type_name);

        m_ObjTypeMap.insert(pair<string, int>(type_name, id));
        ++id;
    }

    LOG_POST(Info << "Creating LDS table: " << "object");

    fname = m_LDS_DirName + "//lds_object.db"; 
    m_db.object_db.Open(fname.c_str(),
                    "object",
                    CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "objectattr");
    fname = m_LDS_DirName + "//lds_objectattr.db"; 
    m_db.object_attr_db.Open(fname.c_str(),
                             "objectattr",
                             CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "annotation");
    fname = m_LDS_DirName + "//lds_annotation.db"; 
    m_db.annot_db.Open(fname.c_str(),
                       "annotation",
                       CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "annot2obj");
    fname = m_LDS_DirName + "//lds_annot2obj.db"; 
    m_db.annot2obj_db.Open(fname.c_str(),
                           "annot2obj",
                           CBDB_RawFile::eCreate);

    LOG_POST(Info << "Creating LDS table: " << "seq_id_list");
    fname = m_LDS_DirName + "//lds_seq_id_list.db"; 
    m_db.seq_id_list.Open(fname.c_str(),
                          "seq_id_list",
                          CBDB_RawFile::eCreate);
}


void CLDS_Database::Open()
{
    string fname;

    fname = m_LDS_DirName + "//lds_file.db"; 
    m_db.file_db.Open(fname.c_str(),
                      "file",
                      CBDB_RawFile::eReadWrite);

    fname = m_LDS_DirName + "//lds_objecttype.db"; 
    m_db.object_type_db.Open(fname.c_str(),
                             "objecttype",
                             CBDB_RawFile::eReadWrite);
    x_LoadTypeMap();

    fname = m_LDS_DirName + "//lds_object.db"; 
    m_db.object_db.Open(fname.c_str(),
                        "object",
                        CBDB_RawFile::eReadWrite);

    fname = m_LDS_DirName + "//lds_objectattr.db"; 
    m_db.object_attr_db.Open(fname.c_str(),
                            "objectattr",
                            CBDB_RawFile::eReadWrite);

    fname = m_LDS_DirName + "//lds_annotation.db"; 
    m_db.annot_db.Open(fname.c_str(),
                       "annotation",
                       CBDB_RawFile::eReadWrite);

    fname = m_LDS_DirName + "//lds_annot2obj.db"; 
    m_db.annot2obj_db.Open(fname.c_str(),
                           "annot2obj",
                           CBDB_RawFile::eReadWrite);

    fname = m_LDS_DirName + "//lds_seq_id_list.db"; 
    m_db.seq_id_list.Open(fname.c_str(),
                          "seq_id_list",
                           CBDB_RawFile::eReadWrite);
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
 * Revision 1.8  2003/08/11 20:01:00  kuznets
 * Reworked lds database and file open procedure.
 * Now all db tables are created in separate OS files, not colocate in one
 * lds.db (looks like BerkeleyDB is not really supporting multiple tables in one file)
 *
 * Revision 1.7  2003/08/05 14:32:01  kuznets
 * Reflecting changes in obj_sniff.hpp
 *
 * Revision 1.6  2003/07/09 19:33:40  kuznets
 * Modified databse Open/Create procedures. (Reflecting new sequence id list table)
 *
 * Revision 1.5  2003/06/25 18:30:11  kuznets
 * Fixed bug with creation of object attributes table
 *
 * Revision 1.4  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.3  2003/06/16 15:39:18  kuznets
 * Removing dead code
 *
 * Revision 1.2  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * Revision 1.1  2003/06/03 14:13:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */



