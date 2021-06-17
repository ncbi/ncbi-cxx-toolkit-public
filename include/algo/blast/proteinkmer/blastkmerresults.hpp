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
 *   Results for BLAST-kmer searches
 *
 */

#ifndef BLAST_KMER_RESULTS_HPP
#define BLAST_KMER_RESULTS_HPP


#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include <objmgr/scope.hpp>

#include "blastkmerutils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


/// Vector of pairs of seq-ids and scores
typedef vector< pair<CRef<CSeq_id>, double> > TBlastKmerScoreVector;

/// This class represents the results for one KMER search (one query).
/// The structure is filled in after the search is done.
/// NOTE: recoverable errors (e.g., query shorter than KMER size) will NOT
/// trigger an exception but the CBlastKmerResults for that query will have 
/// an error or warning.  Use the HasError or HasWarning message to check.
class NCBI_XBLAST_EXPORT CBlastKmerResults : public CObject
{
public:
	/// Constructor
	/// @param query Seq-id of the query
	/// @param scores Scores of the result
	/// @param stats Statistics about the results
	/// @param errs warning and messages
	CBlastKmerResults(CConstRef<objects::CSeq_id>     query,
			TBlastKmerPrelimScoreVector& scores,
			BlastKmerStats& stats,
			CRef<CSeqDB> seqdb,
                        const TQueryMessages& errs);

	/// Destructor
	~CBlastKmerResults() {
	}


	/// Get the vector of GIs and scores for the matches
	const TBlastKmerScoreVector& GetScores() const {
		return m_Scores;
	}

	/// Get the results as a TSeqLocVector
	/// @param tsl TSeqLocVector to fill [in/out]
	/// @param scope CScope to use in the TSL [in]
	void GetTSL(TSeqLocVector& tsl, CRef<CScope> scope) const;

	/// Get the statistics for the search.
	const BlastKmerStats& GetStats() const {
		return m_Stats ;
	}

	/// Set the statistics 
	/// @param stats structure of ancillary data.  Will make deep copy.
        void SetStats(BlastKmerStats& stats) {
		m_Stats = stats;
	}

	/// Return Query ID
	CConstRef<CSeq_id> GetSeqId() const {
		return m_QueryId;
	}

        /// Accessor for the error/warning messsages for this query
        /// @param min_severity minimum severity to report errors [in]
        TQueryMessages GetErrors(int min_severity = eBlastSevError) const;

        /// Returns true if there are errors among the results for this object
        bool HasErrors() const;
        /// Returns true if there are warnings among the results for this object
        bool HasWarnings() const;

private:

	/// Saves the matches as TBlastKmerScoreVector.
	/// Also does the lookup of the Seq-id of subject sequences.
	/// @param scores results to be processed and saved. 
	void x_InitScoreVec(TBlastKmerPrelimScoreVector& scores);

	/// Query ID.
	CConstRef<objects::CSeq_id> m_QueryId;
	
	/// Vector of Seq-ids and scores 
	TBlastKmerScoreVector m_Scores;

	/// Ancillary data 
	BlastKmerStats m_Stats;

	/// Database searched
	CRef<CSeqDB> m_SeqDB;

        /// error/warning messages for this query
        TQueryMessages m_Errors;
};

/// This class holds one or more CBlastKmerResults
class NCBI_XBLAST_EXPORT CBlastKmerResultsSet : public CObject
{
public:

	/// data type contained by this container
	typedef CRef<CBlastKmerResults> value_type;

	/// size_type type definition
	typedef vector<value_type>::size_type size_type;

	/// const_iterator type definition
	typedef vector<value_type>::const_iterator const_iterator;

        /// iterator type definition
        typedef vector<value_type>::iterator iterator;

	/// List of query ids.
	typedef vector< CConstRef<objects::CSeq_id> > TQueryIdVector;

        /// Vector of TBlastKmerScoreVector (scores)
	typedef vector<TBlastKmerPrelimScoreVector> TBlastKmerPrelimScoreVectorSet;
	
        /// Vector of KmerStats
	typedef vector<BlastKmerStats> TBlastKmerStatsVector;

	/// Default constructor 
	CBlastKmerResultsSet();
	
	/// Constructor
	/// @param ids vector of Query IDs
	/// @param scores vector of TBlastKmerScoreVector
	/// @param stats vector of Statistics
	/// @param seqdb CSeqDB object for search.
	/// @param msg_vec vector of messages and warnings.
	CBlastKmerResultsSet(TQueryIdVector& ids,
			TBlastKmerPrelimScoreVectorSet& scores,
			TBlastKmerStatsVector& stats,
			CRef<CSeqDB> seqdb,
                        TSearchMessages msg_vec);

	/// Destructor
	~CBlastKmerResultsSet() {
	}


	CBlastKmerResults & operator[](size_type i) {
		return *m_Results[i];
	}

	const CBlastKmerResults & operator[](size_type i) const {
		return *m_Results[i];
	}

	/// Array like acces with CSeq_id indices
	/// @param ident query sequence id [in]
	CRef<CBlastKmerResults> operator[](const CSeq_id &ident);

	/// Returns the number of queries.
	size_t GetNumQueries() const { return m_NumQueries;}

	/// Add an element to m_Results
	/// @param element to add [in]
	void push_back(value_type& element);

private:

	/// @param ids vector of Query IDs
	/// @param scores vector of TBlastKmerScoreVector
	/// @param stats vector of Statistics
	/// @param seqdb CSeqDB object for search.
	/// @param msg_vec vector of messages and warnings.
	void x_Init(TQueryIdVector& ids, 
		TBlastKmerPrelimScoreVectorSet& scores, 
		TBlastKmerStatsVector& stats,
                CRef<CSeqDB> seqdb,
                TSearchMessages msg_vec);

	/// Vectors of results
	vector< CRef<CBlastKmerResults> > m_Results;

	/// Number or queries
	size_t m_NumQueries;
};

/// Empty results (use on error)
/// @param queryVector Query information.
/// @param queryNum zero offset number of query 
/// @param errMsg error message
/// @param severity Severity of messages (e.g., error or warning)
CRef<CBlastKmerResults>
MakeEmptyResults(TSeqLocVector& queryVector, 
       int queryNum, 
       const string& errMsg, 
       EBlastSeverity severity=eBlastSevError);

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* BLAST_KMER_RESULTS_HPP */
