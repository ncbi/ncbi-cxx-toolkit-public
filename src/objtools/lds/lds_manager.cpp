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
#include <bdb/bdb_cursor.hpp>


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
    TFlags flags = 0;
    flags |= recurse == eDontRecurse? fDontRecurse: fRecurseSubDirs;
    flags |= control_sum == eNoControlSum? fNoControlSum: fComputeControlSum;
    flags |= dup_control == eIgnoreDuplicates? fIgnoreDuplicates: fCheckDuplicates;
    Index(flags);
}

void CLDS_Manager::Index(TFlags flags)
{
    auto_ptr<CLDS_Database> lds = x_OpenDB(CLDS_Database::eReadWrite);
    CLDS_Set files_deleted;
    CLDS_Set files_updated;

    CLDS_File aFile(*lds);
    bool recurse = (flags & fRecurseMask) == fRecurseSubDirs;
    bool control_sum = (flags & fControlSumMask) == fComputeControlSum;
    
    aFile.SyncWithDir(m_SourcePath, &files_deleted, &files_updated,
                      recurse, control_sum);

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;

    bool check_dup = (flags & fDupliucatesMask) == fCheckDuplicates;
    CLDS_Object obj(*lds, lds->GetObjTypeMap());
    obj.ControlDuplicateIds(check_dup);
    switch ( flags & fGBReleaseMask ) {
    case fNoGBRelease:
        obj.ControlGBRelease(CLDS_Object::eNoGBRelease);
        break;
    case fForceGBRelease:
        obj.ControlGBRelease(CLDS_Object::eForceGBRelease);
        break;
    default:
        obj.ControlGBRelease(CLDS_Object::eGuessGBRelease);
        break;
    }
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


void CLDS_Manager::DumpTable(const string& table_name)
{
    NcbiCout << "LDS: Dump of table " << table_name << ":" << NcbiEndl;
    Int8 rec_count = 0;
    SLDS_TablesCollection& tables = GetDB().GetTables();
    if ( table_name == "objecttype" ) {
        SLDS_ObjectTypeDB& t = tables.object_type_db;
        NcbiCout << "id\t"
                 << "name\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.object_type << '\t'
                     << t.type_name << '\n';
        }
    }
    else if ( table_name == "file" ) {
        SLDS_FileDB& t = tables.file_db;
        NcbiCout << "id\t"
                 << "name\t"
                 << "format\t"
                 << "time\t"
                 << "CRC\t"
                 << "size\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.file_id << '\t'
                     << t.file_name << '\t'
                     << t.format << '\t'
                     << t.time_stamp << '\t'
                     << t.CRC << '\t'
                     << t.file_size << '\n';
        }
    }
    else if ( table_name == "annot2obj" ) {
        SLDS_Annot2ObjectDB& t = tables.annot2obj_db;
        NcbiCout << "annot\t"
                 << "object\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.annot_id << '\t'
                     << t.object_id << '\n';
        }
    }
    else if ( table_name == "annotation" ) {
        SLDS_AnnotDB& t = tables.annot_db;
        NcbiCout << "annot\t"
                 << "file\t"
                 << "type\t"
                 << "pos\t"
                 << "TSE\t"
                 << "parent\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.annot_id << '\t'
                     << t.file_id << '\t'
                     << t.annot_type << '\t'
                     << t.file_pos << '\t'
                     << t.TSE_object_id << '\t'
                     << t.parent_object_id << '\n';
        }
    }
    else if ( table_name == "object" ) {
        SLDS_ObjectDB& t = tables.object_db;
        NcbiCout << "id\t"
                 << "file\t"
                 << "seqid\t"
                 << "type\t"
                 << "pos\t"
                 << "TSE\t"
                 << "parent\t"
                 << "title\t"
                 << "org\t"
                 << "keyw\t"
                 << "ids\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.object_id << '\t'
                     << t.file_id << '\t'
                     << t.primary_seqid << '\t'
                     << t.object_type << '\t'
                     << t.file_pos << '\t'
                     << t.TSE_object_id << '\t'
                     << t.parent_object_id << '\t'
                     << t.object_title << '\t'
                     << t.organism << '\t'
                     << t.keywords << '\t'
                     << t.seq_ids << '\n';
        }
    }
    else if ( table_name == "seq_id_list" ) {
        SLDS_SeqId_List& t = tables.seq_id_list;
        NcbiCout << "id\t"
                 << "seqid\n";
        CBDB_FileCursor cur(t);
        cur.SetCondition(CBDB_FileCursor::eFirst);
        while (cur.Fetch() == eBDB_Ok) {
            ++rec_count;
            NcbiCout << t.object_id << '\t'
                     << t.seq_id << '\n';
        }
    }
    NcbiCout << "LDS: End of table " << table_name << ": "
             << rec_count << " records" << NcbiEndl;
}


END_SCOPE(objects)
END_NCBI_SCOPE
