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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_ctx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#include <objtools/lds2/lds2.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache_loader.hpp>

#include <dbapi/driver/drivers.hpp>
#include <dbapi/simple/sdbapi.hpp>

#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CDataLoadersUtil::AddArgumentDescriptions(CArgDescriptions& arg_desc,
                                               TLoaders loaders)
{
    arg_desc.SetCurrentGroup("Data source and object manager options");

    // Local Data Store indexes
    if (loaders & fLDS2) {
        arg_desc.AddOptionalKey("lds2", "LDSDatabases",
                                "Comma-separated list of LDS2 databases to use.",
                                CArgDescriptions::eString,
                                CArgDescriptions::fAllowMultiple);
    }

    // BLAST database retrieval
    if (loaders & fBLAST) {
        arg_desc.AddOptionalKey("blastdb", "BlastDatabases",
                                "Comma-separated list of BLAST databases to use. "
                                "Use na: prefix for nucleotide, "
                                "or aa: for protein.",
                                CArgDescriptions::eString,
                                CArgDescriptions::fAllowMultiple);
        if(!arg_desc.Exist("d")) arg_desc.AddAlias("d","blastdb");
    }

    // ASN Cache options
    if (loaders & fAsnCache) {
        arg_desc.AddOptionalKey("asn-cache", "AsnCache",
                                "Comma-separated list of ASN Cache databases to use.",
                                CArgDescriptions::eString,
                                CArgDescriptions::fAllowMultiple);
    }


    // ID retrieval options
    if (loaders & fGenbank) {
        if (loaders & fGenbankOffByDefault) {
            // VR-623:
            // Compromise to be able to use this class for public ASN.1 tools
            if ( !arg_desc.Exist("r")) {
                arg_desc.AddFlag("r",
                                 "Enable remote data retrieval using the Genbank data loader");
                arg_desc.AddAlias("genbank", "r");
            }
        }
        else {
            if(!arg_desc.Exist("nogenbank")) {
                arg_desc.AddFlag("nogenbank",
                                 "Do not use GenBank data loader.");
            }
        }
    }

    if (loaders & fVDB) {
        arg_desc.AddFlag("vdb", "Use VDB data loader.");
        arg_desc.AddFlag("novdb", "Do not use VDB data loader.");
        arg_desc.SetDependency("vdb",
                               CArgDescriptions::eExcludes,
                               "novdb");
        arg_desc.AddOptionalKey("vdb-path", "Path",
                                "Root path for VDB look-up",
                                CArgDescriptions::eString);
    }

    // SRA options
    if (loaders & fSRA) {
        arg_desc.AddFlag("sra", "Add the SRA data loader with no options.");

        arg_desc.AddOptionalKey("sra-acc", "AddSra", "Add the SRA data loader, specifying an accession.",
                                CArgDescriptions::eString);

        arg_desc.AddOptionalKey("sra-file", "AddSra", "Add the SRA data loader, specifying an sra file.",
                                CArgDescriptions::eString);

        arg_desc.SetDependency("sra", CArgDescriptions::eExcludes, "sra-acc");
        arg_desc.SetDependency("sra", CArgDescriptions::eExcludes, "sra-file");
        arg_desc.SetDependency("sra-acc", CArgDescriptions::eExcludes, "sra-file");
    }

    // All remaining arguments as added by the C++ Toolkit application
    // framework are grouped after the above, by default. If the caller
    // doesn't like this, merely set the current group to the desired
    // value (this one, being empty, will be ignored).
    arg_desc.SetCurrentGroup("General application arguments");
}


void CDataLoadersUtil::x_SetupGenbankDataLoader(const CArgs& args,
                                                CObjectManager& obj_mgr,
                                                int& priority)
{
    bool is_public_asn_tools = args.Exist("genbank");

    bool genbank = args.Exist("genbank") && args["genbank"];
    bool nogenbank = args.Exist("nogenbank") && args["nogenbank"];

    if (is_public_asn_tools) {
        nogenbank = !genbank;
    }

    if ( ! nogenbank ) {
        // pubseqos* drivers require this
        DBAPI_RegisterDriver_FTDS();

#ifdef HAVE_PUBSEQ_OS
        // we may require PubSeqOS readers at some point, so go ahead and make
        // sure they are properly registered
        GenBankReaders_Register_Pubseq();
        GenBankReaders_Register_Pubseq2();
#endif

        // In order to allow inserting additional data loaders
        // between the above high-performance loaders and this one,
        // leave a gap in the priority range.
        priority = max(priority, 16000);
        CGBDataLoader::RegisterInObjectManager(obj_mgr,
                                               0,
                                               CObjectManager::eDefault,
                                               priority);

        LOG_POST(Info << "added loader: GenBank: "
                 << " (" << priority << ")");
        ++priority;
    }
}


void CDataLoadersUtil::x_SetupVDBDataLoader(const CArgs& args,
                                            CObjectManager& obj_mgr,
                                            int& priority)
{
    CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();
    bool use_vdb_loader = args.Exist("vdb") && args["vdb"];
    if ( !use_vdb_loader ) {
        string vdbEnabled = reg.Get("Gpipe","enable_vdb");
        if (vdbEnabled == "Y") {
            use_vdb_loader = true;
        }
    }
    if (args.Exist("novdb") && args["novdb"]) {
        use_vdb_loader = false;
    }
    if (use_vdb_loader) {
        string vdbpath = reg.Get("Gpipe", "VDB_PATH");
        if (args.Exist("vdb-path") && args["vdb-path"]) {
            vdbpath = args["vdb-path"].AsString();
        }
        if(vdbpath.empty()){
            CWGSDataLoader::RegisterInObjectManager
                (obj_mgr, CObjectManager::eDefault, priority++);
        } else {
            CWGSDataLoader::SLoaderParams params;
            params.m_WGSVolPath = vdbpath;
            CWGSDataLoader::RegisterInObjectManager
                (obj_mgr,params,CObjectManager::eDefault, priority++);
        }
    }
}


void CDataLoadersUtil::x_SetupSRADataLoader(const CArgs& args,
                                            CObjectManager& obj_mgr,
                                            int& priority)
{
    if(args.Exist("sra") && args["sra"]) {
        CCSRADataLoader::RegisterInObjectManager(obj_mgr,
                                                 CObjectManager::eDefault,
                                                 priority++);
    }
    else if(args.Exist("sra-acc") && args["sra-acc"]) {
        CCSRADataLoader::RegisterInObjectManager(obj_mgr,
                                                 args["sra-acc"].AsString(),
                                                 CObjectManager::eDefault,
                                                 priority++);
    }
    else if(args.Exist("sra-file") && args["sra-file"]) {
        CDirEntry file_entry(args["sra-file"].AsString());
        CCSRADataLoader::RegisterInObjectManager(obj_mgr,
                                                 file_entry.GetDir(),
                                                 file_entry.GetName(),
                                                 CObjectManager::eDefault,
                                                 priority++);
    }
}


void CDataLoadersUtil::x_SetupBlastDataLoader(const CArgs& args,
                                              CObjectManager& obj_mgr,
                                              int& priority)
{
    typedef vector<string> TDbs;

    CArgValue::TStringArray blastdbstr;
    if (args.Exist("blastdb") && args["blastdb"]) {
        blastdbstr = args["blastdb"].GetStringList();
    }

    ITERATE (CArgValue::TStringArray, it, blastdbstr) {
        TDbs dbs;
        NStr::Split(*it, ",", dbs);
        ITERATE(TDbs, i, dbs) {
            string db = NStr::TruncateSpaces(*i);
            if (db.empty()) {
                continue;
            }
            CBlastDbDataLoader::EDbType dbtype(CBlastDbDataLoader::eUnknown);
            if (NStr::StartsWith(db, "na:", NStr::eNocase)) {
                db.erase(0, 3);
                dbtype = CBlastDbDataLoader::eNucleotide;
            } else if (NStr::StartsWith(db, "aa:", NStr::eNocase)) {
                db.erase(0, 3);
                dbtype = CBlastDbDataLoader::eProtein;
            }
            CBlastDbDataLoader::RegisterInObjectManager(obj_mgr,
                                                        db, dbtype, true,
                                                        CObjectManager::eDefault,
                                                        priority);
            LOG_POST(Info << "added loader: BlastDB: "
                     << (dbtype == CBlastDbDataLoader::eNucleotide ?
                         "nucl" : "protein")
                     << ": " << db
                     << " (" << priority << ")");
            ++priority;
        }
    }
}

void CDataLoadersUtil::x_SetupLDS2DataLoader(const CArgs& args,
                                             CObjectManager& obj_mgr,
                                             int& priority)
{
    typedef vector<string> TDbs;

    CArgValue::TStringArray lds2dbstr;
    if (args.Exist("lds2") && args["lds2"]) {
        lds2dbstr = args["lds2"].GetStringList();
    }

    ITERATE (CArgValue::TStringArray, it, lds2dbstr) {
        TDbs dbs;
        NStr::Split(*it, ",", dbs);
        ITERATE(TDbs, i, dbs) {
            string db = NStr::TruncateSpaces(*i);
            if (db.empty()) {
                continue;
            }
            db = CDirEntry::CreateAbsolutePath(db);
            db = CDirEntry::NormalizePath(db);
            if ( !CFile(db).Exists() ) {
                ERR_POST(Warning << "LDS2 path not found: omitting: " << db);
                continue;
            }

            string alias("lds2_");
            alias += NStr::NumericToString(priority);
            CLDS2_DataLoader::RegisterInObjectManager(obj_mgr,
                                                      db,
                                                      -1 /* fasta_flags */,
                                                      CObjectManager::eDefault,
                                                      priority);
            LOG_POST(Info << "added loader: LDS2: " << db
                     << " (" << priority << ")");
            ++priority;
        }
    }
}

void CDataLoadersUtil::x_SetupASNCacheDataLoader(const CArgs& args,
                                                 CObjectManager& obj_mgr,
                                                 int& priority)
{
    typedef vector<string> TDbs;

    CArgValue::TStringArray asn_cache_str;
    CNcbiRegistry& reg = CNcbiApplication::Instance()->GetConfig();
    string cachePaths = reg.Get("ASN_CACHE","ASN_CACHE_PATH");
    if(cachePaths.size() > 0) {
        asn_cache_str.push_back(cachePaths);
        LOG_POST(Info << "From config: ASNCache:ASN_CACHE_PATH " << cachePaths);
    }

    if (args.Exist("asn-cache") && args["asn-cache"]) {
        asn_cache_str = args["asn-cache"].GetStringList();
    }
    ITERATE (CArgValue::TStringArray, it, asn_cache_str) {
        TDbs dbs;
        NStr::Split(*it, ",", dbs);
        ITERATE(TDbs, i, dbs) {
            string db = NStr::TruncateSpaces(*i);
            if (db.empty()  ||  NStr::EqualNocase(db, "NONE")) {
                continue;
            }
            if (CDir(db).Exists()) {
                try {
                    CAsnCache_DataLoader::RegisterInObjectManager
                        (obj_mgr, db,
                         CObjectManager::eDefault,
                         priority);
                    LOG_POST(Info << "added loader: ASNCache: " << db
                             << " (" << priority << ")");
                    ++priority;
                }
                catch (CException& e) {
                    ERR_POST(Error << "failed to add ASN cache path: "
                             << db << ": " << e);
                }
            } else {
                ERR_POST(Warning << "ASNCache path not found: omitting: " << db);
            }
        }
    }
}


void CDataLoadersUtil::SetupGenbankDataLoader(const CArgs& args,
                                              CObjectManager& obj_mgr)
{
    int priority = 0;
    x_SetupGenbankDataLoader(args, obj_mgr, priority);
}


void CDataLoadersUtil::SetupObjectManager(const CArgs& args,
                                          CObjectManager& obj_mgr,
                                          TLoaders loaders)
{
    //
    // Object Manager Setup
    //
    // Data loaders are configured at priority 1 through N for the
    // first N data loaders other than ID, and ID at priority 16,000
    // (except in the unlikely event of N being that large).

    int priority = 10;

    //
    // Local Data Store: highest priority
    //
    // Justification:
    // A local storage is often set up with altered
    // sequence annotation, and thus may be preferred over
    // less specialized data stores.
    if (loaders & fLDS2) {
        x_SetupLDS2DataLoader(args, obj_mgr, priority);
    }

    //
    // ASN Cache: 2nd priority
    //
    // Justification:
    // The ASN Cache provides very fast access to annotated
    // sequences, and should be consulted first, with the exception
    // of any locally altered sequence annotation.
    if (loaders & fAsnCache) {
        x_SetupASNCacheDataLoader(args, obj_mgr, priority);
    }

    //
    // BLAST database retrieval: 3rd priority
    //
    // Justification:
    // Any BLAST databases provide very fast access to sequences,
    // but this data loader does not provide annotation and has
    // limited descriptors (basically, just deflines and taxid).
    if (loaders & fBLAST) {
        x_SetupBlastDataLoader(args, obj_mgr, priority);
    }

    // SRA
    if (loaders & fSRA) {
        x_SetupSRADataLoader(args, obj_mgr, priority);
    }

    // VDB data loader
    if (loaders & fVDB) {
        x_SetupVDBDataLoader(args, obj_mgr, priority);
    }

    //
    // ID retrieval: last priority
    //
    // Justification:
    // Although ID provides access to fully annotated sequences,
    // present and historic/archival, the retrieval performance is
    // significantly lower when compared to the above sources.
    // (There may be exceptions such as when sequences exist in
    // and ICache service, but even then, performance is comparable
    // so there is limited penalty for being last priority.)
    if (loaders & fGenbank) {
        x_SetupGenbankDataLoader(args, obj_mgr, priority);
    }
}


CRef<objects::CScope> CDataLoadersUtil::GetDefaultScope(const CArgs& args)
{
    CRef<objects::CObjectManager> object_manager = objects::CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *object_manager);
    CRef<objects::CScope> scope(new objects::CScope(*object_manager));
    scope->AddDefaults();
    return scope;
}



END_SCOPE(objects)
END_NCBI_SCOPE
