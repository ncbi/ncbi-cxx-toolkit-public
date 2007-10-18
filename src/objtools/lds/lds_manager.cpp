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
 * Author: Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description: 
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/lds/lds_manager.hpp>
#include <objtools/lds/lds_files.hpp>
#include <objtools/lds/lds_object.hpp>
#include <objtools/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_LDS_Mgr


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CLDS_Manager::CLDS_Manager(const string& source_path, const string& db_path, const string& db_alias)
    : m_SourcePath(source_path), m_DbPath(db_path),
      m_DbAlias(db_alias)
{
    if (m_DbPath.empty())
        m_DbPath = m_SourcePath;
}
  

CLDS_Manager::~CLDS_Manager()
{
}


auto_ptr<CLDS_Database> CLDS_Manager::x_OpenDB(CLDS_Database::EOpenMode omode)
{
    auto_ptr<CLDS_Database> lds(new CLDS_Database(m_DbPath, m_DbAlias));
    try {
        lds->Open(omode);
    } catch (...) {
        if (omode == CLDS_Database::eReadWrite)
            CLDS_Manager::sx_CreateDB(*lds);
        else 
            throw;
    }
    return lds;
}


/* static */
void CLDS_Manager::sx_CreateDB(CLDS_Database& lds)
{
    lds.Create(); 

    SLDS_TablesCollection& db = lds.GetTables();

    //
    // Upload the object type DB
    //
    LOG_POST_X(1, Info << "Uploading " << "objecttype");

    CLDS_CoreObjectsReader  core_reader;

    const CLDS_CoreObjectsReader::TCandidates& cand 
                                = core_reader.GetCandidates();
    
    int id = 1;
    db.object_type_db.object_type = id;
    db.object_type_db.type_name = "FastaEntry";
    db.object_type_db.Insert();

    LOG_POST_X(2, Info << "  " << "FastaEntry");

    ++id;
    ITERATE(CLDS_CoreObjectsReader::TCandidates, it, cand) {
        string type_name = it->type_info.GetTypeInfo()->GetName();

        db.object_type_db.object_type = id;
        db.object_type_db.type_name = type_name;
        db.object_type_db.Insert();

        LOG_POST_X(3, Info << "  " << type_name);

        ++id;
    }

    lds.LoadTypeMap();
}

void CLDS_Manager::Index(ERecurse           recurse,
                         EComputeControlSum control_sum,
                         EDuplicateId       dup_control)
{
    auto_ptr<CLDS_Database> lds = x_OpenDB(CLDS_Database::eReadWrite);
    CLDS_Set files_deleted;
    CLDS_Set files_updated;

    CLDS_File aFile(*lds);
    bool rec = (recurse == eRecurseSubDirs);
    bool control = (control_sum == eComputeControlSum);
    aFile.SyncWithDir(m_SourcePath, &files_deleted, &files_updated, rec, control);

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;

    CLDS_Object obj(*lds, lds->GetObjTypeMap());
    obj.ControlDuplicateIds(dup_control == eCheckDuplicates);
    obj.DeleteCascadeFiles(files_deleted, 
                           &objects_deleted, &annotations_deleted);
    obj.UpdateCascadeFiles(files_updated);


    lds->Sync();
}

CLDS_Database& CLDS_Manager::GetDB()
{
    if ( !m_lds_db.get() ) {
        m_lds_db.reset( x_OpenDB(CLDS_Database::eReadOnly).release());
    }
    return *m_lds_db;
}
CLDS_Database* CLDS_Manager::ReleaseDB()
{
    if (m_lds_db.get() )
        return m_lds_db.release();
    return  x_OpenDB(CLDS_Database::eReadOnly).release();
}

void CLDS_Manager::DeleteDB()
{
    CDir dir(m_DbPath);
    if (dir.Exists())
        dir.Remove();
}

END_SCOPE(objects)
END_NCBI_SCOPE
