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
 *   Class to print results
 *
 */

#include <ncbi_pch.hpp>
#include <algo/blast/format/blast_async_format.hpp>


// Global mutex
CFastMutex blastProcessGuard;


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

CBlastAsyncFormatThread::~CBlastAsyncFormatThread()
{
}

void 
CBlastAsyncFormatThread::QueueResults(int batchNumber,
	vector<SFormatResultValues> results)
{
	if (m_Done == true)
		NCBI_THROW(CException, eUnknown, "QueueResults called after Finalize");
        if (m_ResultsMap.find(batchNumber) != m_ResultsMap.end())
        {
		string message = "Duplicate batchNumber entered: " + NStr::NumericToString(batchNumber);
		NCBI_THROW(CException, eUnknown, "message");
	}
	blastProcessGuard.Lock();
	m_ResultsMap.insert(std::pair<int, vector<SFormatResultValues>>(batchNumber, results));
	blastProcessGuard.Unlock();
	m_Semaphore.Post();
}


void 
CBlastAsyncFormatThread::Finalize()
{
	blastProcessGuard.Lock();
        m_Done=true;
	blastProcessGuard.Unlock();
	m_Semaphore.Post();
}

void
CBlastAsyncFormatThread::Join()
{
	if(m_Done == false)
		Finalize();

	CThread::Join();	
}


void* CBlastAsyncFormatThread::Main(void)
{
	const int kVecSize=5000;  // Large array so we should not wrap around.
	vector<vector<SFormatResultValues>> results_v;
	results_v.resize(kVecSize);
	int currNum=0;
	int lastNum=0;
	while (1)
	{
		m_Semaphore.Wait();
		blastProcessGuard.Lock();  
		for(std::map<int, vector<SFormatResultValues>>::iterator itr=m_ResultsMap.begin(); itr != m_ResultsMap.end(); itr++)
        	{
			if (itr->first < currNum)
				continue;
			else if (itr->first > currNum)
				break;

               		results_v[currNum%kVecSize].swap(itr->second);
			currNum++;
		}
		blastProcessGuard.Unlock();

		for (int index=lastNum; index<currNum; ++index)
		{
               		for(vector<SFormatResultValues>::iterator vecitr=results_v[index%kVecSize].begin(); 
				vecitr != results_v[index%kVecSize].end(); vecitr++)
                	{
				ITERATE(CSearchResultSet, result, *((*vecitr).blastResults))
					(*vecitr).formatter->PrintOneResultSet(**result, (*vecitr).qVec);
                	}
			results_v[index%kVecSize].clear();
        	}
		lastNum=currNum;
		if (m_Done == true) // All worker threads done.
		{
			if (m_ResultsMap.size() != currNum)
			{  // More results that have been loaded but not printed.
				m_Semaphore.Post();
				continue;
			}
			else
				break;
		}
	}
	return (void*) NULL;
}

