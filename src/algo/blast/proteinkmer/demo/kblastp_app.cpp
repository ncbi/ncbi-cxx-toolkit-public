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
 *   Search kmer index
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/kblastp_args.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/format/blast_async_format.hpp>
#include <math.h>

#include <objtools/readers/fasta.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>

#include "kblastthr.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

/////////////////////////////////////////////////////////////////////////////
//  CBlastKmerApplication::


class CBlastKmerApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    /// Get the command-line arguments
    CRef<CKBlastpAppArgs> m_CmdLineArgs;
};

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CBlastKmerApplication::Init(void)
{

	m_CmdLineArgs.Reset(new CKBlastpAppArgs());

	HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideVersion | fHideDryRun);

	SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

/////////////////////////////////////////////////////////////////////////////
//  Run demo

int CBlastKmerApplication::Run(void)
{
	int retval = 0;

    	const bool kIsProtein = true;
    	SDataLoaderConfig dlconfig(kIsProtein);
    	CBlastInputSourceConfig iconfig(dlconfig, objects::eNa_strand_other, false, GetArgs()["parse_deflines"].AsBoolean());
    	CBlastFastaInputSource fasta_input(GetArgs()["query"].AsInputFile(), iconfig);

	// EXPERIMENT
	const int kBatchSize=10000;
    	CBlastInput blast_input(&fasta_input, kBatchSize);

	const CArgs& args = GetArgs();
	CRef<CBlastOptionsHandle> opts_handle;
	opts_handle.Reset(&*m_CmdLineArgs->SetOptions(args));

	CRef<CBlastOptions> newOpts = opts_handle->GetOptions().Clone();
	CRef<CBlastpKmerOptionsHandle> kmerOptsHndle(new CBlastpKmerOptionsHandle(newOpts));
	kmerOptsHndle->SetThresh(m_CmdLineArgs->GetJaccardDistance());
	kmerOptsHndle->SetMinHits(m_CmdLineArgs->GetMinHits());
	kmerOptsHndle->SetCandidateSeqs(m_CmdLineArgs->GetCandidateSeqs());

        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
	string db = db_args->GetDatabaseName();

	CRef<CBlastKmerOptions> options(new CBlastKmerOptions());

        CBlastAsyncFormatThread* formatThr = new CBlastAsyncFormatThread();
	formatThr->Run();

	CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
	int numThreads = m_CmdLineArgs->GetNumThreads();
	typedef CBlastKmerThread* CBlastKmerThreadPtr;

	CBlastKmerThreadPtr *thr = new CBlastKmerThreadPtr[numThreads];

	for (int i=0; i<numThreads; ++i)
	{
		thr[i] = new CBlastKmerThread(blast_input, db_args, kmerOptsHndle, fmt_args, m_CmdLineArgs->GetOutputStream(), 
			formatThr, GetArgs()["parse_deflines"].AsBoolean());
		thr[i]->Run();
	}

	for (int i=0; i<numThreads; ++i)
	{
		thr[i]->Join();
		// cerr << "Joined: " << i << "\n";
	}
	formatThr->Join();

	delete [] thr;
	return retval;
}	

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBlastKmerApplication::Exit(void)
{
	SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
	// Execute main application function
	return CBlastKmerApplication().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
