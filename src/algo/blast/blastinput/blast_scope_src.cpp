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
#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>
#include <objtools/data_loaders/genbank/id2/reader_id2.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

const char* SDataLoaderConfig::kDefaultProteinBlastDb = "nr";
const char* SDataLoaderConfig::kDefaultNucleotideBlastDb = "nt";

/// @note the database can be overridden at runtime by the DATA_LOADERS entry
/// in the BLAST section of the NCBI configuration file. Allowed values are
/// blastdb, genbank, and none. If this is changed, please update the
/// BLAST+ user manual
void
SDataLoaderConfig::x_Init(SDataLoaderConfig::EConfigOpts options,
                          const string& dbname,
                          bool load_proteins)
{
    m_UseFixedSizeSlices = true;
    m_UseBlastDbs = (options & eUseBlastDbDataLoader) ? true : false;
    m_UseGenbank = (options & eUseGenbankDataLoader) ? true : false;
    if ( !dbname.empty() ) {
        m_BlastDbName.assign(dbname);
    }
    m_IsLoadingProteins = load_proteins;

    CMetaRegistry::SEntry sentry =
        CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    x_LoadDataLoadersConfig(sentry);
    x_LoadBlastDbDataLoaderConfig(sentry);
}

void
SDataLoaderConfig::x_LoadDataLoadersConfig(const CMetaRegistry::SEntry& sentry)
{
    static const string kDataLoadersConfig("DATA_LOADERS");

    if (sentry.registry && 
        sentry.registry->HasEntry("BLAST", kDataLoadersConfig)) {
        const string& kLoaders = sentry.registry->Get("BLAST",
                                                      kDataLoadersConfig);
        if (NStr::FindNoCase(kLoaders, "blastdb") == NPOS) {
            m_UseBlastDbs = false;
        }
        if (NStr::FindNoCase(kLoaders, "genbank") == NPOS) {
            m_UseGenbank = false;
        }
        if (NStr::FindNoCase(kLoaders, "none") != NPOS) {
            m_UseBlastDbs = false;
            m_UseGenbank = false;
        }
    }
    _TRACE("Using data loaders: blastdb " << boolalpha << m_UseBlastDbs
           << "; genbank " << boolalpha << m_UseGenbank);
}

void
SDataLoaderConfig::x_LoadBlastDbDataLoaderConfig
    (const CMetaRegistry::SEntry& sentry)
{
    if ( !m_UseBlastDbs ) {
        m_BlastDbName.clear();
        return;
    }

    // if the database was already specified via the API, don't override it.
    if ( !m_BlastDbName.empty() ) {
        return;
    }

    static const string kProtBlastDbLoaderConfig("BLASTDB_PROT_DATA_LOADER");
    static const string kNuclBlastDbLoaderConfig("BLASTDB_NUCL_DATA_LOADER");

    const string& config_param = m_IsLoadingProteins 
        ? kProtBlastDbLoaderConfig 
        : kNuclBlastDbLoaderConfig;

    if (sentry.registry && sentry.registry->HasEntry("BLAST", config_param)) {
        m_BlastDbName = sentry.registry->Get("BLAST", config_param);
    } else {
        _ASSERT(m_BlastDbName.empty());
        m_BlastDbName = m_IsLoadingProteins 
            ? kDefaultProteinBlastDb 
            : kDefaultNucleotideBlastDb;
    }
    _ASSERT( !m_BlastDbName.empty() );
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
                (*m_ObjMgr, dbname, dbtype, m_Config.m_UseFixedSizeSlices,
                 CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet)
                 .GetLoader()->GetName();
        _ASSERT( !m_BlastDbLoaderName.empty() );
    } catch (const CSeqDBException& e) {
        // if the database isn't found, ignore the exception as the
        // remote BLAST database data loader will be tried next
        if (e.GetMsg().find("No alias or index file found ") != NPOS) {
            ERR_POST(Info << "Error initializing local BLAST database "
                          << "data loader: '" << e.GetMsg() << "'");
            _TRACE("Error initializing local BLAST database "
                          << "data loader: '" << e.GetMsg() << "'");
        }
        m_BlastDbLoaderName = CRemoteBlastDbDataLoader::RegisterInObjectManager
                (*m_ObjMgr, dbname, dbtype, m_Config.m_UseFixedSizeSlices,
                 CObjectManager::eNonDefault, CObjectManager::kPriority_NotSet)
                 .GetLoader()->GetName();
        _ASSERT( !m_BlastDbLoaderName.empty() );
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
                    (*m_ObjMgr, db_handle, m_Config.m_UseFixedSizeSlices,
                     CObjectManager::eNonDefault).GetLoader()->GetName();
            _ASSERT( !m_BlastDbLoaderName.empty() );

        } catch (const exception& e) {

            // in case of error when initializing the BLAST database, just
            // ignore the exception as the remote BLAST database data loader
            // will be the fallback (just issue a warning)
            ERR_POST(Info << "Error initializing local BLAST database data "
                             << "loader: '" << e.what() << "'");
            const CBlastDbDataLoader::EDbType dbtype = 
                db_handle->GetSequenceType() == CSeqDB::eProtein
                ? CBlastDbDataLoader::eProtein
                : CBlastDbDataLoader::eNucleotide;
            try {
                m_BlastDbLoaderName =
                    CRemoteBlastDbDataLoader::RegisterInObjectManager
                        (*m_ObjMgr, db_handle->GetDBNameList(), dbtype,
                         m_Config.m_UseFixedSizeSlices,
                         CObjectManager::eNonDefault,
                         CObjectManager::kPriority_NotSet)
                         .GetLoader()->GetName();
                _ASSERT( !m_BlastDbLoaderName.empty() );
            } catch (const CSeqDBException& e) {
                ERR_POST(Info << "Error initializing remote BLAST database "
                              << "data loader: " << e.GetMsg());
            }
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

/// Counts the number of BLAST database data loaders registered in the object
/// manager. This is needed so that the priorities of the BLAST databases can
/// be adjusted accordingly when multiple BLAST database data loaders are added
/// to CScope objects (@sa AddDataLoaders)
static int s_CountBlastDbDataLoaders()
{
    int retval = 0;
    CObjectManager::TRegisteredNames loader_names;
    CObjectManager::GetInstance()->GetRegisteredNames(loader_names);
    ITERATE(CObjectManager::TRegisteredNames, loader_name, loader_names) {
        if (NStr::Find(*loader_name, "BLASTDB") != NPOS) {
            retval++;
        }
    }
    return retval;
}

void 
CBlastScopeSource::AddDataLoaders(CRef<objects::CScope> scope)
{
    const int blastdb_loader_priority = 
        kBlastDbLoaderPriority + (s_CountBlastDbDataLoaders() - 1);

    // Note that these priorities are needed so that the CScope::AddXXX methods
    // don't need a specific priority (the default will be fine).
    if (!m_BlastDbLoaderName.empty()) {
        scope->AddDataLoader(m_BlastDbLoaderName, blastdb_loader_priority);
    } 
    if (!m_GbLoaderName.empty()) {
        scope->AddDataLoader(m_GbLoaderName, kGenbankLoaderPriority);
    }
}

CRef<CScope> CBlastScopeSource::NewScope()
{
    CRef<CScope> retval(new CScope(*m_ObjMgr));
    AddDataLoaders(retval);
    return retval;
}

void
CBlastScopeSource::RevokeBlastDbDataLoader()
{
    if (!m_BlastDbLoaderName.empty()) {
        CObjectManager::GetInstance()->RevokeDataLoader(m_BlastDbLoaderName);
        m_BlastDbLoaderName.clear();
    }
}

bool
SDataLoaderConfig::operator==(const SDataLoaderConfig& rhs) const
{
    if (this == &rhs) {
        return true;
    }
    if (m_UseGenbank != rhs.m_UseGenbank) {
        return false;
    }
    if (m_UseBlastDbs != rhs.m_UseBlastDbs) {
        return false;
    }
    if (m_IsLoadingProteins != rhs.m_IsLoadingProteins) {
        return false;
    }
    if (m_BlastDbName != rhs.m_BlastDbName) {
        return false;
    }
    return true;
}

bool
SDataLoaderConfig::operator!=(const SDataLoaderConfig& rhs) const
{
    return !(*this == rhs);
}

END_SCOPE(blast)
END_NCBI_SCOPE
