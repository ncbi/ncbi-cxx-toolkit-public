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
 * Author:  Thomas W Rackers
 *
 * File Description:
 *   Copy CSeqDB accession-to-OID records from ISAM to SQLite form.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_writer/writedb_sqlite.hpp>

#include <ctime>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#include <sys/types.h>
#include <sys/stat.h>


/////////////////////////////////////////////////////////////////////////////
//  CSeqdb2CreateApplication::


class CSeqdb2CreateApplication : public CNcbiApplication
{
private:
    typedef CSeqDBSqlite::TOid TOid;
    typedef CSeqDBSqlite::SVolInfo SVolInfo;

    CWriteDB_Sqlite* m_sqldb = NULL;
    bool m_quiet = false;
    bool m_verbose = false;

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    int processVolumeTable(
            const vector<string>& paths,
            CSeqDB::ESeqType seqType
    );
    int processBatch(
            TOid&   oid,
            TOid    lastOid,
            int     num,
            CSeqDB& seqdb
    );
    int processSuperBatch(
            const int     bigBatch,
            const string& seqdbName,
            const string& sqldbName,
            const TOid    firstOid,
            const int     numOids,
            const int     batchSize
    );
};


/////////////////////////////////////////////////////////////////////////////
//  Initialize arguments


void CSeqdb2CreateApplication::Init(void)
{
    // Create command-line argument descriptions class.
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context.
    arg_desc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "seqdb2_create"
    );

    // Add required keys.
    arg_desc->AddKey(
            "db",
            "in_db",
            "Input database (SeqDB)",
            CArgDescriptions::eString
    );

    arg_desc->AddKey(
            "out",
            "out_db",
            "Output database (SQLite)",
            CArgDescriptions::eString
    );

    // Add default keys.
    arg_desc->AddDefaultKey(
            "batch_size",
            "BatchSize",
            "Batch size",
            CArgDescriptions::eInteger,
            "10000"
    );
    arg_desc->SetConstraint(
            "batch_size",
            new CArgAllow_Integers(100, 1000000)
    );

    // Add flags.
    arg_desc->AddFlag(
            "quiet",
            "Suppress output, overrides -verbose",
            true
    );
    arg_desc->AddFlag(
            "verbose",
            "Show verbose output",
            true
    );

    // Setup arg.descriptions for this application.
    SetupArgDescriptions(arg_desc.release());
}


// Create the volume info table.

int CSeqdb2CreateApplication::processVolumeTable(
        const vector<string>& paths,
        CSeqDB::ESeqType seqType
)
{
    list<SVolInfo> vols;

    // Get info for each DB volume.
    int vol = 0;
    for (auto it : paths) {
        // Get modification time of volume.
        // Use ".nin" or ".pin" file to get date.
        string s = it + ((seqType == CSeqDB::eProtein) ? ".pin" : ".nin");
        struct stat attr;
        int rc = stat(s.c_str(), &attr);
        if (rc != 0) {
            cerr << "Cannot stat " << it << ", errno = " << errno << endl;
            continue;
        }
        time_t modtime = attr.st_mtime;
        CSeqDB* seqvol = new CSeqDB(it, CSeqDB::eUnknown);
        // Get number of OIDs for each volume.
        int oids = seqvol->GetNumOIDs();
        delete seqvol;
        vols.push_back(SVolInfo(s, modtime, vol, oids));
        ++vol;
    }
    m_sqldb->CreateVolumeTable(vols);

    return vol;
}


// Process a batch of records as a single transaction.

int CSeqdb2CreateApplication::processBatch(
        TOid& oid,
        TOid lastOid,
        int num,
        CSeqDB& seqdb
)
{
    int added = 0;
    m_sqldb->BeginTransaction();
    for (int b = 0; b < num; ++b) {
        if (oid >= lastOid) break;
        list<CRef<CSeq_id> > seqids = seqdb.GetSeqIDs((int) oid);
        added += m_sqldb->InsertEntries(oid, seqids);
        ++oid;
    }
    m_sqldb->EndTransaction();

    return added;
}


// Process the input database in "super-batches".  This prevents the
// input and output databases from consuming too much memory for large
// databases like nr and nt.

int CSeqdb2CreateApplication::processSuperBatch(
        const int bigBatch,
        const string& seqdbName,
        const string& sqldbName,
        const TOid firstOid,
        const TOid lastOid,
        const int batchSize
)
{
    // Open input SeqDB database.
    CSeqDB seqdb(seqdbName, CSeqDB::eUnknown);

    // Open output SQLite database.
    m_sqldb = new CWriteDB_Sqlite(sqldbName);
    m_sqldb->SetCacheSize(16L << 20);       // 16 MiB

    time_t tic, toc;    // wall clock time, resolution 1 sec
    TOid oid = firstOid;
    int added;
    int totalAdded = 0;
    const int totalAddedLimit = 10 * 1000 * 1000;   // 10 million

    // Process and time each batch.
    // Exit loop when either last OID is processed, or when memory usage
    // hits preset limit.  In the latter case, this method will be called
    // again.
    int batch = 0;
    while (totalAdded < totalAddedLimit  &&  oid < lastOid) {
        tic = time(NULL);
        added = processBatch(oid, lastOid, batchSize, seqdb);
        toc = time(NULL);
        totalAdded += added;
        if (m_verbose) {
            cout << "Batch " << bigBatch << ":" << ++batch
                    << " ends at OID " << (int) oid << ", "
                    << added << " rows added in "
                    << difftime(toc, tic) << " sec" << endl;
        }
    }

    // Finalize.
    delete m_sqldb;

    // Return oid where we're leaving off.
    return oid;
}


/////////////////////////////////////////////////////////////////////////////
//  Run application


int CSeqdb2CreateApplication::Run(void)
{
    // Initialize SQLite.
    CSQLITE_Global::Initialize();

    // Get arguments.
    const CArgs& args = GetArgs();

    m_quiet = args["quiet"].AsBoolean();
    if (m_quiet) {
        m_verbose = false;
    } else {
        m_verbose = args["verbose"].AsBoolean();
    }

    string seqfn = args["db"].AsString();
    int batchSize = args["batch_size"].AsInteger();
    string sqlfn = args["out"].AsString();

    if (!m_quiet) {
        cout << "Database:     " << args["db"].AsString() << endl;
    }

    // Open input database temporarily.
    CSeqDB* seqdb = new CSeqDB(seqfn, CSeqDB::eUnknown);

    // Get volumes paths.
    vector<string> paths;
    seqdb->FindVolumePaths(
            paths,
            true    // recursive
    );

    // Get number of OIDS and sequence type.
    CSeqDB::ESeqType seqType = seqdb->GetSequenceType();
    int numOids = seqdb->GetNumOIDs();
    if (!m_quiet) {
        cout << "Title:        " << seqdb->GetTitle() << endl;
        cout << "# OIDs:       " << numOids << endl;
        cout << "Batch size:   " << batchSize << endl;
    }
    delete seqdb;   // done with input database for the moment

    // Initialize SQLite package.
    m_sqldb = new CWriteDB_Sqlite(sqlfn);
    m_sqldb->SetCacheSize(16L << 20);   // 16 MB

    // Create volume info table.
    int nvols = processVolumeTable(paths, seqType);
    if (!m_quiet) {
        cout << "# volumes:    " << nvols << endl;
    }

    // Close database for now.
    delete m_sqldb;

    // Step through all OIDs, starting from 0.
    // processSuperBatch will return when either all OIDs are exhausted,
    // or when more than 10 million rows have been added since it was called.
    // This method reopens the database, then closes it again after processing
    // the superbatch, freeing memory before starting the next one.
    TOid oid = (TOid) 0;
    int bigBatchNum = 1;
    while ((int) oid < numOids) {
        oid = processSuperBatch(
                bigBatchNum,
                seqfn,
                sqlfn,
                oid,
                (TOid) numOids,
                batchSize
        );
        ++bigBatchNum;
    }

    // Re-initialize SQLite package.
    m_sqldb = new CWriteDB_Sqlite(sqlfn);
    m_sqldb->SetCacheSize(16 * 1024 * 1024);

    if (!m_quiet) {
        cout << "Creating index... " << flush;
    }

    // Create index on accession column.
    time_t tic = time(NULL);
    m_sqldb->CreateIndex();
    time_t toc = time(NULL);
    int dt = difftime(toc, tic);
    if (!m_quiet) {
        if (dt < 1) {
            cout << "in <1 sec" << endl;
        } else {
            cout << "in " << dt << " sec" << endl;
        }
    }

    // Finish up.
    delete m_sqldb;

    // Finalize SQLite.
    CSQLITE_Global::Finalize();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSeqdb2CreateApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    // Execute main application function; change argument list to
    // (argc, argv, 0, eDS_Default, 0) if there's no point in trying
    // to look for an application-specific configuration file.
    return CSeqdb2CreateApplication().AppMain(argc, argv);
}
