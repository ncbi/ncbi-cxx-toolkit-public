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
 * Authors:  Sergey Satskiy
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <misc/netstorage/netstorage.hpp>

#include "netstorage_gc.hpp"
#include "netstorage_gc_version.hpp"
#include "netstorage_gc_database.hpp"
#include "netstorage_gc_exception.hpp"



USING_NCBI_SCOPE;


CNetStorageGCApp::CNetStorageGCApp(void) :
    CNcbiApplication(),
    m_DeleteCount(0), m_TotalCount(0), m_RemoteErrors(0)
{
    CVersionInfo        version(NCBI_PACKAGE_VERSION_MAJOR,
                                NCBI_PACKAGE_VERSION_MINOR,
                                NCBI_PACKAGE_VERSION_PATCH);
    CRef<CVersion>      full_version(new CVersion(version));

    full_version->AddComponentVersion("Expected NetStorage DB structure",
                                      NETSTORAGE_GC_EXPECTED_DB_STRUCTURE,
                                      0, 0);

    SetVersion(version);
    SetFullVersion(full_version);
}


void CNetStorageGCApp::Init(void)
{
    auto_ptr<CArgDescriptions>  arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Garbage collector for NetStorage database "
                              "and backend storages");
    arg_desc->AddFlag("verbose", "Be verbose (messages on stdout)");
    arg_desc->AddFlag("dry", "Dry run, no real cleanup is done");

    SetupArgDescriptions(arg_desc.release());
}


int CNetStorageGCApp::Run(void)
{
    GetDiagContext().Extra()
        .Print("_type", "startup")
        .Print("info", "versions")
        .Print("netstorage_gc", NETSTORAGE_GC_VERSION)
        .Print("db_structure", NETSTORAGE_GC_EXPECTED_DB_STRUCTURE)
        .Print("build", NETSTORAGE_GC_BUILD_DATE);


    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   reg = GetConfig();

    bool                    verbose = args["verbose"];
    bool                    dryrun = args["dry"];
    CNetStorageGCDatabase   db(reg, verbose);

    // Check the DB structure version
    int                     db_ver = db.GetDBStructureVersion();
    if (db_ver != NETSTORAGE_GC_EXPECTED_DB_STRUCTURE) {
        string  message("NetStorage meta info database "
                        "structure version mismatch. Expected: " +
                        NStr::NumericToString(
                                    NETSTORAGE_GC_EXPECTED_DB_STRUCTURE) +
                        ", found: " + NStr::NumericToString(db_ver));
        ERR_POST(message);
        cerr << message << endl;
        return 1;
    }

    vector<string>          candidates = db.GetGCCandidates();
    m_TotalCount = candidates.size();

    if (!candidates.empty()) {
        // It was decided not to perform this check because it can lead to
        // unwilling behavior. Here is an example:
        // - a NetStorage server is configured with service A
        // - an object was created with service A (thus with meta DB)
        // - the server is reconfigured and A replaced with B
        // - the object is expired
        // - when a request for exists comes to NetStorage it extracts the
        //   service from the locator - it will be A - and checks if meta DB
        //   access is required. The answer is NO becase A != B. So the server
        //   goes to the backend storage to check the existance. The server
        //   reports back: object not found
        // - the garbage collector expects the ObjectExpired exception but
        //   receives 'not found' condition. That stops the GC utility.
        // x_CheckExpiration(candidates, verbose);
        try {
            x_RemoveObjects(candidates, verbose, db, dryrun);
        } catch (...) {
            x_PrintFinishCounters(verbose);
            throw;
        }
    }

    x_PrintFinishCounters(verbose);
    return 0;
}


// Paranoid check.
// Basically it checks the same condition as was checked in the SELECT
// statement when remove candidates were selected. The difference is that the
// check is done via an instance of a NetStorage server.
// Note: not used anymore
void  CNetStorageGCApp::x_CheckExpiration(const vector<string> &  locators,
                                          bool  verbose)
{
    const CNcbiRegistry &   reg = GetConfig();
    string                  service_name = reg.GetString("netstorage_server",
                                                         "service", "");
    if (service_name.empty()) {
        string      message = "[netstorage_server]/service is not specified";
        cerr << message << endl;
        NCBI_THROW(CNetStorageGCException, eNetStorageServiceNotFound, message);
    }

    CNetStorage     nst("nst=" + service_name + "&client_name=netstorage_gc");
    for (vector<string>::const_iterator  k = locators.begin();
            k != locators.end(); ++k) {
        try {
            if (verbose)
                cout << "Checking expiration via NetStorage server for " << *k << endl;
            nst.Exists(*k);

            string  msg = "Expected expired object; "
                          "NetStorage reports it is not";
            cerr << msg << ": " << *k << endl;
            NCBI_THROW(CNetStorageGCException, eExpirationExpected, msg);
        } catch (const CNetStorageGCException &  ex) {
            throw;
        } catch (...) {
            cerr << "Exception while checking expiration for object "
                 << *k << endl;
            throw;
        }
    }
}


void  CNetStorageGCApp::x_RemoveObjects(const vector<string> &  locators,
                                        bool  verbose,
                                        CNetStorageGCDatabase &  db,
                                        bool  dryrun)
{
    CDirectNetStorage net_storage(
                        GetConfig(),
                        CNetICacheClient(CNetICacheClient::eAppRegistry),
                        NULL, kEmptyStr);
    CRef<CRequestContext>   ctx;
    bool                    error;

    for (vector<string>::const_iterator  k = locators.begin();
            k != locators.end(); ++k) {

        const string &      hit_id = CDiagContext::GetRequestContext()
                                                                .SetHitID();
        error = true;
        try {
            if (verbose)
                cout << "Removing backend storage object " << *k << endl;

            ctx.Reset(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            ctx->SetHitID(hit_id);
            GetDiagContext().PrintRequestStart()
                            .Print("action", "backend_remove")
                            .Print("locator", *k);

            if (!dryrun)
                net_storage.Remove(*k);

            ctx->SetRequestStatus(200);
            GetDiagContext().PrintRequestStop();
            ctx.Reset();
            GetDiagContext().SetRequestContext(NULL);

            error = false;
        } catch (const CException &  ex) {
            ERR_POST(ex);

            ctx->SetRequestStatus(500);
            GetDiagContext().PrintRequestStop();
            ctx.Reset();
            GetDiagContext().SetRequestContext(NULL);

            cerr << "Exception while removing backend object "
                 << *k << endl;
        } catch (const std::exception &  ex ) {
            ERR_POST(ex.what());

            ctx->SetRequestStatus(500);
            GetDiagContext().PrintRequestStop();
            ctx.Reset();
            GetDiagContext().SetRequestContext(NULL);

            cerr << "std::exception while removing backend object "
                 << *k << endl;
        } catch (...) {
            ERR_POST(k_UnknownException);

            ctx->SetRequestStatus(500);
            GetDiagContext().PrintRequestStop();
            ctx.Reset();
            GetDiagContext().SetRequestContext(NULL);

            cerr << "Unknown exception while removing backend object "
                 << *k << endl;
        }

        // Cleaning MS SQL db only if the remote storage deletion passed OK
        if (error == false) {
            db.RemoveObject(*k, dryrun, hit_id);
            ++m_DeleteCount;
        } else
            ++m_RemoteErrors;
    }
}


void  CNetStorageGCApp::x_PrintFinishCounters(bool  verbose)
{
    GetDiagContext().Extra()
        .Print("_type", "finish")
        .Print("total_count", NStr::NumericToString(m_TotalCount))
        .Print("deleted_count", NStr::NumericToString(m_DeleteCount))
        .Print("remote_errors", NStr::NumericToString(m_RemoteErrors));

    if (verbose)
        cout << "Total candidates: " << m_TotalCount << endl
             << "Deleted: " << m_DeleteCount << endl
             << "Remote errors: " << m_RemoteErrors << endl;
}


int  main(int  argc, const char *  argv[])
{
    CDiagContext::SetOldPostFormat(false);

    return CNetStorageGCApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}


