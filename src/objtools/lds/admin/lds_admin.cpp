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
 * File Description:  LDS database maintanance implementation.
 *
 */

#include <objtools/lds/admin/lds_admin.hpp>
#include <objtools/lds/admin/lds_files.hpp>
#include <objtools/lds/admin/lds_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CLDS_Management::CLDS_Management(CLDS_Database& db)
: m_lds_db(db)
{}


void CLDS_Management::SyncWithDir(const string& dir_name, 
                                  ERecurse recurse,
                                  EComputeControlSum control_sum)
{
    CLDS_Set files_deleted;
    CLDS_Set files_updated;
    SLDS_TablesCollection& db = m_lds_db.GetTables();

    CLDS_File aFile(db);
    bool rec = (recurse == eRecurseSubDirs);
    bool control = (control_sum == eComputeControlSum);
    aFile.SyncWithDir(dir_name, &files_deleted, &files_updated, rec, control);

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;

    CLDS_Object obj(db, m_lds_db.GetObjTypeMap());
    obj.DeleteCascadeFiles(files_deleted, 
                           &objects_deleted, &annotations_deleted);
    obj.UpdateCascadeFiles(files_updated);
    m_lds_db.Sync();
}


CLDS_Database* 
CLDS_Management::OpenCreateDB(const string&      dir_name,
                              const string&      db_name,
                              bool*              is_created,
                              ERecurse           recurse,
                              EComputeControlSum control_sum)
{
    CLDS_Database* db = new CLDS_Database(dir_name, db_name);
    try {
        db->Open();
        *is_created = false;
    } 
    catch (CBDB_ErrnoException& ex)
    {
        // Failed to open: file does not exists.
        // Force the construction
        LOG_POST("Warning: trying to open LDS database:" << ex.what());

        CLDS_Management admin(*db);
        admin.Create();
        admin.SyncWithDir(dir_name, recurse, control_sum);
        *is_created = true;
    }
    return db;
}

void CLDS_Management::Create() 
{ 
    m_lds_db.Create(); 

    SLDS_TablesCollection& db = m_lds_db.GetTables();

    //
    // Upload the object type DB
    //
    LOG_POST(Info << "Uploading " << "objecttype");

    CLDS_CoreObjectsReader  core_reader;

    const CLDS_CoreObjectsReader::TCandidates& cand 
                                = core_reader.GetCandidates();
    
    int id = 1;
    db.object_type_db.object_type = id;
    db.object_type_db.type_name = "FastaEntry";
    db.object_type_db.Insert();

    LOG_POST(Info << "  " << "FastaEntry");

    ++id;
    ITERATE(CLDS_CoreObjectsReader::TCandidates, it, cand) {
        string type_name = it->type_info.GetTypeInfo()->GetName();

        db.object_type_db.object_type = id;
        db.object_type_db.type_name = type_name;
        db.object_type_db.Insert();

        LOG_POST(Info << "  " << type_name);

        ++id;
    }

    m_lds_db.LoadTypeMap();

}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/04/20 14:52:25  rsmith
 * Name fl -> aFile since fl is a macro (?!) on Macs.
 *
 * Revision 1.8  2003/10/09 18:12:27  kuznets
 * Some functionality of lds migrated into lds_admin
 *
 * Revision 1.7  2003/10/09 16:45:28  kuznets
 * + explicit Sync() call
 *
 * Revision 1.6  2003/10/06 20:16:20  kuznets
 * Added support for sub directories and option to disable CRC32 for files
 *
 * Revision 1.5  2003/08/11 20:01:48  kuznets
 * Reflecting lds.hpp header change
 *
 * Revision 1.4  2003/08/06 20:13:05  kuznets
 * Fixed LDS database creation bug CLDS_Management::OpenCreateDB always reported TRUE
 * in is_created parameter
 *
 * Revision 1.3  2003/06/25 18:28:40  kuznets
 * + CLDS_Management::OpenCreateDB(...)
 *
 * Revision 1.2  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.1  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * ===========================================================================
*/
