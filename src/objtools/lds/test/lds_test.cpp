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
* Authors:  Yuri Kapustin, Victor Joukov
*
* File Description:
*   Test memory leaks in C++ object manager
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/lds/lds_admin.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqloc/Seq_id.hpp>



USING_NCBI_SCOPE;


class CTestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void CTestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "LDS test program");

    arg_desc->AddOptionalKey("mklds", "mklds", "Index LDS directory",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ldsdir", "ldsdir", "Existing LDS directory to use",
                             CArgDescriptions::eString);

    arg_desc->AddExtra(0, kMax_UInt, "IDs to check", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


const string kSplignLdsDb ("splign.ldsdb");

string GetLdsDbDir(const string& fasta_dir)
{
    string lds_db_dir (fasta_dir);
    const char sep (CDirEntry::GetPathSeparator());
    const size_t fds (fasta_dir.size());
    if(fds > 0 && fasta_dir[fds-1] != sep) {
        lds_db_dir += sep;
    }
    lds_db_dir += "_SplignLDS_";
    return lds_db_dir;
}


int CTestApplication::Run(void)
{
    const CArgs & args (GetArgs());

    if(args["mklds"]) {

        // create LDS DB and exit
        string fa_dir (args["mklds"].AsString());
        if(! CDirEntry::IsAbsolutePath(fa_dir)) {
            string curdir  (CDir::GetCwd());
            const char sep (CDirEntry::GetPathSeparator());
            const size_t curdirsize (curdir.size());
            if(curdirsize && curdir[curdirsize-1] != sep) {
                curdir += sep;
            }
            fa_dir = curdir + fa_dir;
        }
        const string lds_db_dir (GetLdsDbDir(fa_dir));
        CLDS_Database ldsdb (lds_db_dir, kSplignLdsDb);
        CLDS_Management ldsmgt (ldsdb);
        ldsmgt.Create();
        ldsmgt.SyncWithDir(fa_dir,
                           CLDS_Management::eRecurseSubDirs,
                           CLDS_Management::eNoControlSum);
        return 0;
    }
    
    // use LDS DB
    CRef<CObjectManager> objmgr (CObjectManager::GetInstance());

    const string fasta_dir (args["ldsdir"].AsString());
    const string ldsdb_dir (GetLdsDbDir(fasta_dir));
    CLDS_Database* ldsdb (new CLDS_Database(ldsdb_dir, kSplignLdsDb));
    ldsdb->Open();
    CLDS_DataLoader::RegisterInObjectManager(*objmgr, *ldsdb,
                                             CObjectManager::eDefault);
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    for (size_t n = 1; n <= args.GetNExtra(); ++n) {
        CSeq_id seqid1 (args[n].AsString());
        cout << "The length of " << seqid1.GetSeqIdString() << " is:\t"
            << sequence::GetLength(seqid1, scope.GetNonNullPointer())
            << endl;
    }

    return 0;
}


void CTestApplication::Exit(void)
{
    SetDiagStream(0);
}

int main(int argc, const char* argv[])
{
    CTestApplication theApp;
    return theApp.AppMain(argc, argv, 0, eDS_Default, 0);
}
