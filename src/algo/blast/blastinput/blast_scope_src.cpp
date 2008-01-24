#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_scope_src.cpp
 * Defines CBlastScopeSource class to create properly configured CScope
 * objects.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

const char* SDataLoaderConfig::kDefaultProteinBlastDb = "prot_dbs";
const char* SDataLoaderConfig::kDefaultNucleotideBlastDb = "nucl_dbs";

void
SDataLoaderConfig::x_Init(SDataLoaderConfig::EConfigOpts options,
                          const string& dbname,
                          bool load_proteins)
{
    m_UseBlastDbs = (options & eUseBlastDbDataLoader) ? true : false;
    m_UseGenbank = (options & eUseGenbankDataLoader) ? true : false;
    m_BlastDbName.assign(dbname);
    m_IsLoadingProteins = load_proteins;

    CMetaRegistry::SEntry sentry =
        CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);

    if (sentry.registry && sentry.registry->HasEntry("BLAST", "DATA_LOADERS")) {
        string loaders = sentry.registry->Get("BLAST", "DATA_LOADERS");
        if (NStr::FindNoCase(loaders, "blastdb") == NPOS) {
            m_UseBlastDbs = false;
        }
        if (NStr::FindNoCase(loaders, "genbank") == NPOS) {
            m_UseGenbank = false;
        }
        if (NStr::FindNoCase(loaders, "none") != NPOS) {
            m_UseBlastDbs = false;
            m_UseGenbank = false;
        }
    }
}

CBlastScopeSource::CBlastScopeSource(bool load_proteins /* = true */,
                                     CObjectManager* objmgr /* = NULL */)
 : m_Config(load_proteins)
{
    m_ObjMgr.Reset(objmgr ? objmgr : CObjectManager::GetInstance());
    x_InitBlastDatabaseDataLoader(m_Config.m_BlastDbName, 
                                  m_Config.m_IsLoadingProteins
                                  ? CBlastDbDataLoader::eProtein
                                  : CBlastDbDataLoader::eNucleotide);
    x_InitGenbankDataLoader();
}

CBlastScopeSource::CBlastScopeSource(const SDataLoaderConfig& config,
                                     CObjectManager* objmgr /* = NULL */)
 : m_Config(config)
{
    m_ObjMgr.Reset(objmgr ? objmgr : CObjectManager::GetInstance());
    x_InitBlastDatabaseDataLoader(m_Config.m_BlastDbName, 
                                  m_Config.m_IsLoadingProteins
                                  ? CBlastDbDataLoader::eProtein
                                  : CBlastDbDataLoader::eNucleotide);
    x_InitGenbankDataLoader();
}

CBlastScopeSource::CBlastScopeSource(CRef<CSeqDB> db_handle,
                                     CObjectManager* objmgr /* = NULL */)
 : m_Config(static_cast<bool>(db_handle->GetSequenceType() == CSeqDB::eProtein))
{
    m_ObjMgr.Reset(objmgr ? objmgr : CObjectManager::GetInstance());
    x_InitBlastDatabaseDataLoader(db_handle);
    x_InitGenbankDataLoader();
}

void
CBlastScopeSource::x_InitBlastDatabaseDataLoader(const string& dbname,
                                                 EDbType dbtype)
{
    if ( !m_Config.m_UseBlastDbs ) {
        return;
    }

    try {

        m_BlastDbLoaderName = CBlastDbDataLoader::RegisterInObjectManager
                (*m_ObjMgr, dbname, dbtype,
                 CObjectManager::eNonDefault).GetLoader()->GetName();

    } catch (const CSeqDBException& e) {

        // if the database isn't found, ignore the exception as the Genbank
        // data loader will be the fallback (just issue a warning)

        if (e.GetMsg().find("No alias or index file found ") != NPOS) {
            ERR_POST(Warning << "Error initializing local BLAST database data "
                             << "loader: '" << e.GetMsg() << "'");
        }

    }
}

void
CBlastScopeSource::x_InitBlastDatabaseDataLoader(CRef<CSeqDB> db_handle)
{
    if ( !m_Config.m_UseBlastDbs ) {
        return;
    }

    if (db_handle.Empty()) {
        ERR_POST(Warning << "No BLAST database handle provided");
    } else {
        try {

            m_BlastDbLoaderName = CBlastDbDataLoader::RegisterInObjectManager
                    (*m_ObjMgr, db_handle,
                     CObjectManager::eNonDefault).GetLoader()->GetName();

        } catch (const exception& e) {

            // in case of error when initializing the BLAST database, just
            // ignore the exception as the Genbank data loader will be the 
            // fallback (just issue a warning)
            ERR_POST(Warning << "Error initializing local BLAST database data "
                             << "loader: '" << e.what() << "'");
        }
    }
}

void
CBlastScopeSource::x_InitGenbankDataLoader()
{
    if ( !m_Config.m_UseGenbank ) {
        return;
    }

    try {
        CRef<CReader> reader(new CId2Reader);
        reader->SetPreopenConnection(false);
        m_GbLoaderName = CGBDataLoader::RegisterInObjectManager
            (*m_ObjMgr, reader, CObjectManager::eNonDefault)
            .GetLoader()->GetName();
    } catch (const CException& e) {
        m_GbLoaderName.erase();
        ERR_POST(Warning << "Error initializing Genbank data loader: " 
                         << e.GetMsg());
    }
}

CRef<CScope> CBlastScopeSource::NewScope()
{
    CRef<CScope> retval(new CScope(*m_ObjMgr));

    // Note that these priorities are needed so that the CScope::AddXXX methods
    // don't need a specific priority (the default will be fine).
    if (!m_BlastDbLoaderName.empty()) {
        retval->AddDataLoader(m_BlastDbLoaderName, kBlastDbLoaderPriority);
    } 
    if (!m_GbLoaderName.empty()) {
        retval->AddDataLoader(m_GbLoaderName, kGenbankLoaderPriority);
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE
