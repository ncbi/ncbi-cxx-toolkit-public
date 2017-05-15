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
 *   BLAST-kmer searches
 *
 */

#ifndef BLAST_KMER_KMER_HPP
#define BLAST_KMER_KMER_HPP


#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include "blastkmerresults.hpp"
#include "blastkmeroptions.hpp"
#include "mhfile.hpp"
#include "blastkmerutils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

/// Class to perform a KMER-BLASTP search.
/// To run, first call the constructor, then RunSearches, then 
/// access results through CBlastKmerResultsSet.
/// The Run method is deprecated and will be removed.
/// A few notes/caveats:
/// The CBlastKmerOptions constructor can be called and the resulting
/// object used as input.
/// The string kmerfile should be kEmptyStr so that the volumes for
/// the database in the seqdb parameter will be used. 
/// If kEmptyStr is not used for the kmerfile, then, for nr, it would 
/// be "nr.00 nr.01 nr.02 nr.03" etc.
/// These should correspond exactly to the database as they rely on the oids in
/// the database.  If the kmerfiles are derived from the BLAST database
/// then the BLASTDB paths etc. will be respected for the kmerfiles.
/// NOTE: recoverable errors (e.g., query shorter than KMER size) will NOT
/// trigger an exception but the CBlastKmerResults for that query will have 
/// an error or warning.  Use the HasError or HasWarning message to check.
class NCBI_XBLAST_EXPORT CBlastKmer : public CObject, public CThreadable
{
public:
	/// Constructor
	/// Processes all proteins in TSeqLocVector
	/// @param query_vector specifes one protein query sequence [in]
	/// @param options sets kblastp parameters. [in]
	/// @param seqdb CSeqDB pointer for database to be searched.
	/// @param kmerfile assume same names as seqdb volumes if kEmptyStr [in]
	CBlastKmer(TSeqLocVector& query_vector,
		CRef<CBlastKmerOptions> options,
		CRef<CSeqDB> seqdb,
		string kmerfile=kEmptyStr);

	/// Constructor
	/// @param query specifes one protein query sequence [in]
	/// @param options sets kblastp parameters. [in]
	/// @param dbname name of a BLAST database [in]
	CBlastKmer(SSeqLoc& query,
		CRef<CBlastKmerOptions> options,
		const string& dbname);

	/// Destructor 
	~CBlastKmer() {}

	/// Performs search on one or more queries
	/// Performs search on one or more queries
	/// @throw CInputException if the query has no KMERs
	CRef<CBlastKmerResultsSet> Run();

	/// @deprecated Use Run method instead
	/// Performs search on one or more queries
	/// Just calls Run method.
	/// @throw CInputException if the query has no KMERs
	CRef<CBlastKmerResultsSet> RunSearches();
	

	/// Limits output by GILIST
	/// @param list CRef<CSeqDBGiList> to limit by [in]
	void SetGiListLimit(CRef<CSeqDBGiList> list) {m_GIList=list;}

	/// Limits output by negative GILIST
	/// @param list CRef<CSeqDBNegativeList> to limit by [in]
	void SetGiListLimit(CRef<CSeqDBNegativeList> list) {m_NegGIList=list;}

private:

	/// Preprocess query to sequence hashes.
	/// @param query_seq contains query in ncbistdaa [in]
	/// @param seq_hash All hash values for query [out]
	/// @param query_LSH_hashes LSH values [out]
	/// @param num_hashes Number of hash functions [in]
	/// @param rows_per_band [in]
	/// @param samples how many samples to check (Buhler) [in]
	/// @param a Array of num_hash hash values [in]
	/// @param b Array of num_hash hash values [in]
	/// @param kmerNum size of kmer [in]
	/// @param alphabetChoice 0 is 15 letter, 1 is 10 letter alphabet [in]
	/// @param version which version of the kmer index to use [in]
	/// @param kvalues Buhler LSH points [in]
	/// @param badMers Overrepresented KMERs [in]
	/// @throw CInputException if the query has no KMERs
	void x_ProcessQuery(const string& query_seq, 
		SOneBlastKmerSearch& kmerSearch,
                const SBlastKmerParameters& kmerParams,
		uint32_t *a, 
		uint32_t *b, 
		vector < vector <int> >& kvalues,
		vector<int> badMers);

	/// Search individual kmer file.
	/// @param query_hash All hash values for query [in]
	/// @param query_LSH_hashes LSH values [in]
	/// @param filename basename of kmer files. [in]
	/// @param score_vector results vector [out]
	/// @param kmer_stats ancillary information about run [out]
	void x_RunKmerFile(const vector < vector <uint32_t> >& query_hash, const vector < vector <uint32_t> >& query_LSH_hash, CMinHashFile& mhfile, TBlastKmerPrelimScoreVector& score_vector, BlastKmerStats& kmer_stats);

	
        /// Search multiple queries.
	CRef<CBlastKmerResultsSet>
	x_SearchMultipleQueries(int firstQuery, int numQuery, const SBlastKmerParameters& kmerParams, uint32_t *a, uint32_t *b, vector < vector<int> >& kValues, vector<int> badMers);
	

private:

	/// Holds the query seqloc and scope
	TSeqLocVector m_QueryVector;

	/// Specifies values for some options (e.g., threshold)
	CRef<CBlastKmerOptions> m_Opts;

	/// CSeqDB for BLAST db.
	CRef<CSeqDB> m_SeqDB;

	/// Name of the kmer files.
	vector<string> m_KmerFiles;

	/// GIList to limit search by.
	CRef<CSeqDBGiList> m_GIList;

	/// Negative GIList to limit search by.
	/// Only one of the gilist or negative GIlist should be set.
	CRef<CSeqDBNegativeList> m_NegGIList;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* BLAST_KMER_KMER_HPP */
