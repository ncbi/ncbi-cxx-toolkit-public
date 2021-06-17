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
 *   Class to print results in an asynchronous manner
 *
 */

#ifndef ALGO_BLAST_FORMAT___BLAST_ASYNC_FORMAT__HPP
#define ALGO_BLAST_FORMAT___BLAST_ASYNC_FORMAT__HPP

#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);


/// Contains query, results and CBlastFormat for one batch
struct SFormatResultValues {

	CRef<CBlastQueryVector> qVec; ///< Queries
	CRef<CSearchResultSet> blastResults; ///< Results
	CRef<CBlastFormat> formatter; ///< Information for formatting
	SFormatResultValues(CRef<CBlastQueryVector> qv, CRef<CSearchResultSet> br, CRef<CBlastFormat> fmt)
		: qVec(qv), blastResults(br), formatter(fmt) {}
};

/////////////////////////////////////////////////////////////////////////////
/// Run as separate thread and format results.
class NCBI_XBLASTFORMAT_EXPORT CBlastAsyncFormatThread : public CThread
{
public:
    CBlastAsyncFormatThread() 
    : m_ResultsMap(), m_Done(false), m_Semaphore(0, kMax_Int)
   {
   }

   /// Queue results for printing.
   /// Will throw if called after call to Finalize or if a duplicate
   /// batchNumber is entered.  
   /// Batch numbers should start at zero and increase.  Missing numbers NOT allowed.
   /// @param batchNumber orders how the results are printed.  
   ///  Numbering starts at zero and missing values not allowed.
   /// @param results data needed for formatting
   void QueueResults(int batchNumber, vector<SFormatResultValues> results); 

   /// Close queue for printing.  No calls to QueueResults allowed after this.
   void Finalize();

   /// Calls Finalize (if not already called) then CThread::Join();
   /// Should only be called if QueueResults will no longer be called.
   void Join();

protected:
    virtual ~CBlastAsyncFormatThread(void);

    virtual void* Main(void);
private:

    // Prohibit copy constructor and assignment operator
    CBlastAsyncFormatThread(const CBlastAsyncFormatThread&);
    CBlastAsyncFormatThread& operator= (const CBlastAsyncFormatThread&);

    std::map<int, vector<SFormatResultValues>> m_ResultsMap;

    bool m_Done;

    CSemaphore m_Semaphore;
};

#endif /* ALGO_BLAST_FORMAT___BLAST__ASYNC_FORMAT__HPP */

