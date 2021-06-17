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
 * Author: Tom Madden
 *
 * File Description:
 *   Produce database for kmer (minhash) search.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/proteinkmer/blastkmerutils.hpp>
#include <algo/blast/proteinkmer/blastkmerindex.hpp>

#ifdef _OPENMP
#include <omp.h>
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBlastKmerBuildIndexApplication::


class CBlastKmerBuildIndexApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CBlastKmerBuildIndexApplication::Init(void)
{
	HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideVersion | fHideDryRun);
	
	unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

	arg_desc->AddKey("db", "database_name", 
					 "BLAST database to read and create",
					 CArgDescriptions::eString);
	
	arg_desc->AddDefaultKey("samples", "buhler_samples", 
					 "Number of samples to check",
					 CArgDescriptions::eInteger,
					 "30");
	
	 arg_desc->AddDefaultKey("kmer", "kmerNum",
                        "kmer size", CArgDescriptions::eInteger, "5");
	
	arg_desc->AddDefaultKey("width", "width_data", 
					 "Width of data arrays.  4 for an int, 2 for short(recommmended), 1 for byte",
					 CArgDescriptions::eInteger, "2");
	
	arg_desc->AddDefaultKey("threads", "number_threads", 
					 "Number of threads to use.",
					 CArgDescriptions::eInteger, "1");
	
	arg_desc->AddDefaultKey("hashes", "number_hashes", 
					 "Number of Hash functions to use.",
					 CArgDescriptions::eInteger, "32");
	
	arg_desc->AddDefaultKey("alphabet", "alphabet_choice", 
					 "0 (zero) for 15 letter alphabet, 1 for 10 letter alphabet",
					 CArgDescriptions::eInteger, "0");
	
	arg_desc->AddDefaultKey("kversion", "index_version", 
					 "1 for older LSH, 2 for Buhler LSH, 3 for one hash method",
					 CArgDescriptions::eInteger, "3");
	
	 arg_desc->AddOptionalKey("output", "Outputfile",
                        "Index files", CArgDescriptions::eString);
	
 	arg_desc->AddDefaultKey("logfile",
         	"LogInformation",
        	"File for logging errors",
         	CArgDescriptions::eOutputFile,
        	"mkkblastindex.log",
         	CArgDescriptions::fAppend);

	arg_desc->SetUsageContext("mkkblastindex", "Index for protein kmer search");
	
	SetupArgDescriptions(arg_desc.release());
}

/////////////////////////////////////////////////////////////////////////////
//  Build a KMER index.
//

int CBlastKmerBuildIndexApplication::Run(void)
{

	SetDiagPostLevel(eDiag_Warning);
    	SetDiagPostPrefix("mkkblastindex");

	int retval = 0;
	// blast database of sequences
	CRef<CSeqDB> seqdb(new CSeqDB(GetArgs()["db"].AsString(), CSeqDB::eProtein));

	int samples = GetArgs()["samples"].AsInteger();
	
	int kmerNum = GetArgs()["kmer"].AsInteger();

	int dataWidth = GetArgs()["width"].AsInteger();

	int numThreads = GetArgs()["threads"].AsInteger();

	int numHashes = GetArgs()["hashes"].AsInteger();

	int alphabet = GetArgs()["alphabet"].AsInteger();

	int version = GetArgs()["kversion"].AsInteger();

	CNcbiOstream * logFile = & (GetArgs()["logfile"].HasValue()
                   ? GetArgs()["logfile"].AsOutputFile()
                   : cout);

	*logFile <<  CTime(CTime::eCurrent).AsString() << ": Producing indices for " << GetArgs()["db"].AsString() << " using " << numThreads << " threads" << endl;

	try {
		CBlastKmerBuildIndex build_index(seqdb, kmerNum, numHashes, samples, dataWidth, alphabet, version);
		build_index.Build(numThreads);
	}
        catch (const CSeqDBException& e) {              
        	*logFile << "CSeqDB Database error: " << e.GetMsg() << endl;   
        	retval = 1;
    	}                                  
        catch (const blast::CMinHashException& e)  {
        	*logFile << "CMinHash error: " << e.GetMsg() << endl;        
                retval = 1;
        }
        catch (const CFileException& e)  {
        	*logFile << "File error: " << e.GetMsg() << endl;
                retval = 1;
        }
        catch (const std::ios::failure&) {                    
        	*logFile << "mkkblastindex failed to write output" << endl;
        	retval = 1;
    	}                                                    
    	catch (const std::bad_alloc&) {                     
        	*logFile << "mkkblastindex ran out of memory" << endl;
        	retval = 1;
    	}                                            
	catch (...)  {
                *logFile << "Unknown error" << endl;
                retval = 1;
	}
	return retval;
}
	

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastKmerBuildIndexApplication::Exit(void)
{
	SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
	// Execute main application function
	return CBlastKmerBuildIndexApplication().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
