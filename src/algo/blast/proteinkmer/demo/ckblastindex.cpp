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
 * Authors:  Tom Madden
 *
 * File Description:
 *   Application for sanity check of KMER indices (produced by mkkblastindex).
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>



USING_NCBI_SCOPE;
USING_SCOPE(blast);


/////////////////////////////////////////////////////////////////////////////
//  CCkblastindexApplication::


class CCkblastindexApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CCkblastindexApplication::Init(void)
{
    // // Set error posting and tracing on maximum
    // SetDiagTrace(eDT_Enable);
    // SetDiagPostFlag(eDPF_All);
    // SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Runs sanity checks on KMER indices from mkkblastindex");

	arg_desc->AddKey("db", "database_name",
                                         "BLAST database to check",
                                         CArgDescriptions::eString);


    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey("logfile",
         "LogInformation", 
	"File for logging errors",
         CArgDescriptions::eOutputFile,
	"ckblastindex.log",
	 CArgDescriptions::fAppend);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CCkblastindexApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    string database = GetArgs()["db"].AsString();
  
    string error_msg="";
    CRef<CSeqDB> seqdb(new CSeqDB(database, CSeqDB::eProtein));
    int status=BlastKmerVerifyIndex(seqdb, error_msg);
    CNcbiOstream *logFile = & (args["logfile"].AsOutputFile());
    *logFile << "Started checking " << database << " at " <<  CTime(CTime::eCurrent).AsString() << endl;
    *logFile << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;

    if (status != 0)
    {
	*logFile << "ERROR: " << database << " failed validation" << endl;
	if (error_msg != "")
		*logFile << error_msg << endl;
    }
    *logFile << "Finished checking " << database << " at " <<  CTime(CTime::eCurrent).AsString() << endl;
    *logFile << endl;

    return status;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CCkblastindexApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function; change argument list to
    // (argc, argv, 0, eDS_Default, 0) if there's no point in trying
    // to look for an application-specific configuration file.
    return CCkblastindexApplication().AppMain(argc, argv);
}
