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
 *   Utilities for BLAST kmer
 *
 */

#ifndef BLAST_KMER_UTILS_HPP
#define BLAST_KMER_UTILS_HPP


#include <corelib/ncbistd.hpp>
#include <util/range.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/core/blast_message.h>

#include "mhfile.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);

// From http://primes.utm.edu/lists/small/millions
#define PKMER_PRIME 1048583

/// Structure for ancillary data on KMER search.
/// Describes how many hits were examined etc.
struct BlastKmerStats {
	/// Constructor
	/// @param hit_count How many hits to the hash array
	/// @param jd_count how often was Jaccard distance calculated.
	/// @param jd_oid_count how many OIDs for Jaccard distance.
	/// @param oids_considered Number OIDs considered as candidates.
	/// @param total_matches Number of matches returned.
	/// @param seqs Number of database sequences considered
	BlastKmerStats(int hit_count=0, 
			int jd_count=0, 
			int jd_oid_count=0, 
			int oids_considered=0, 
			int total_matches=0,
			int seqs=0)
	   : hit_count(hit_count), 
	     jd_count(jd_count), 
	     jd_oid_count(jd_oid_count), 
	     oids_considered(oids_considered), 
	     total_matches(total_matches),
	     num_sequences(seqs)
	{
	}
	/// How many hits to the hash array were there?
        int hit_count; 
	/// How often was the Jaccard distance calculated.
        int jd_count; 
	/// How many OIDs was the Jaccard distance calculated for.
        int jd_oid_count; 
	/// How many OIDs were considered as candidates.
        int oids_considered; 
	/// How many matches returned.
	int total_matches;
	/// Number of database sequences considered (in this volume)
	int num_sequences;
};

struct SBlastKmerParameters {
	/// Constructor
	///
	SBlastKmerParameters(int numHashes,
                            int rowsPerBand,
                            int samples,
                            int kmerNum,
                            int alphabetChoice,
                            int version,
                            int chunkSize=150)
	: numHashes(numHashes), rowsPerBand(rowsPerBand), samples(samples),
		kmerNum(kmerNum), alphabetChoice(alphabetChoice), version(version),
		chunkSize(chunkSize)
        {
        }
	/// Number of hash functions per signature
	int numHashes;
	/// Number of values sampled from signature.
	int rowsPerBand;
	/// Number of samples of query signature are made?
	int samples;
	/// number of letters in KMER.
	int kmerNum;
	/// 15 or 10 letter alphabet (0 for 15, 1 for 10).
	int alphabetChoice;
	/// Version of index used (0 indicates default).
	int version;
        /// size of a query chunk to process (default is 150).
        int chunkSize; 
};

/// Vector of pairs of database OIDs and scores.
/// ONLY for use during KMER search, not presentation of results
/// or communication with other modules (BLAST or not).
typedef vector< pair<uint32_t, double> > TBlastKmerPrelimScoreVector;

struct SOneBlastKmerSearch {

	/// Constructor
	/// @param numFiles Number of (alias) files to search
	SOneBlastKmerSearch(int numFiles=0)
           : qSeqid(), queryHash(), queryLSHHash()
	{
		scoreVector.resize(numFiles);
		kmerStatsVector.resize(numFiles);
		status=0;
		severity=eBlastSevInfo;
		errDescription=kEmptyStr;
	}
	
	/// Seqid of the query.
	CRef<CSeq_id> qSeqid;	
        /// Hashes for one query (multiple chunks)
        vector < vector <uint32_t> > queryHash;
        /// LSH Hashes for one query (multiple chunks)
        vector < vector <uint32_t> > queryLSHHash;
        /// Scores for one query
        vector< TBlastKmerPrelimScoreVector > scoreVector;
        /// Stats for one query
        vector< BlastKmerStats > kmerStatsVector;
	/// Status of the query (0 is good, otherwise an error has occurred)
	int status;
	/// Error or warning (only use if status is non-zero).
	EBlastSeverity severity;
	/// Error description
	string errDescription;
};

/// Get KMERs for a given sequence using a compressed alphabet.
///
///@param query_sequence string with one sequence [in]
///@param do_seg Should the sequence be segged (not recommended) [in]
///@param range portion of sequence to be processed [in]
///@param kmerNum size of kmer [in]
///@param alphabetChoice 0 is 15 letter, 1 is 10 letter alphabet [in]
///@return set of unsigned ints for the kmers.
NCBI_XBLAST_EXPORT
set<uint32_t> BlastKmerGetKmerSet(const string& query_sequence, bool do_seg, TSeqRange& range, int kmerNum, int alphabetChoice);
	
/// Get KMERs for a given sequence using a compressed alphabet.
/// This version can read in overrepresented KMERs and extend them by one.
///@param query_sequence string with one sequence [in]
///@param range portion of sequence to be processed [in]
///@param kmerNum size of kmer [in]
///@param alphabetChoice 0 is 15 letter, 1 is 10 letter alphabet [in]
///@param badMers Overrepresented KMERs [in]
///@return set of unsigned ints for the kmers.
NCBI_XBLAST_EXPORT
set<uint32_t> BlastKmerGetKmerSet2(const string& query_sequence, TSeqRange& range, int kmerNum, int alphabetChoice, vector<int> badMers);
	
/// Simplified version of BlastKmerGetKmerSet.  Intended
/// for gathering statistics on KMERS in the database.
///
///@param query_sequence string with one sequence [in]
///@param kmerNum size of kmer [in]
///@return param kmerCount Count population of different KMERS
///@return param kmerCount Count population of different KMERS one longer than kmerNum
///@param alphabetChoice 0 is 15 letter, 1 is 10 letter alphabet [in]
///@param perQuery COunt kmers per query or total in database.
///@return set of unsigned ints for the kmers.
NCBI_XBLAST_EXPORT
set<uint32_t> BlastKmerGetKmerSetStats(const string& query_sequence, int kmerNum, map<string, int>& kmerCount, map<string, int>& kmerCountPlus, int alphabetChoice, bool perQuery);

/// Breaks a sequences up into chunks if the sequences is above a certain length.
/// Each chunk has an overlap with the adjoining chunks.
/// Breaking sequences up into chunks makes the minhash procedure much more
/// effective two sequences of different lengths are being compared.
///
/// @param length Total length of sequence being broken up [in]
/// @param range_v Vector of ranges to be filled in [out]
/// @param chunkSize number of residues in sequence chunk [in]
/// @return number of chunks.  Should be integer more than zero.  Zero or less indicates an error.
NCBI_XBLAST_EXPORT
int BlastKmerBreakUpSequence(int length, vector<TSeqRange>& range_v, int chunkSize);

/// Creates translation table for compressed alphabets
///
/// @param trans_table Translation table [out]
/// @param alphabetChoice 0 is 15 letter, 1 is 10 letter alphabet [in]
///
NCBI_XBLAST_EXPORT
void BlastKmerGetCompressedTranslationTable(vector<Uint1>& trans_table, int alphabetChoice);

/// Calculates the number of differences between two minhash arrays.
/// Used to decide whether two arrays are similar enough.  The assumption
/// is made that both arrays are of the same size.
/// @param minhash1 First array [in]
/// @param minhash2 Second array [in]
/// @return distance.  
NCBI_XBLAST_EXPORT
int BlastKmerGetDistance(const vector<uint32_t>& minhash1, const vector<uint32_t>& minhash2);

NCBI_XBLAST_EXPORT
bool minhash_query(const string& query,
                     vector < vector <uint32_t> >& seq_hash,
                     int num_hashes,
                     uint32_t *a,
                     uint32_t *b,
                     int do_seg,
                     int kmerNum,
	    	     int alphabetChoice,
                     int chunkSize);

/// Hash the query for the minimum values;
/// @param query as a ASCII string [in]
/// @param seq_hash hash values for all kmers [out]
/// @param kmerNum number of letters in a KMER [in]
/// @param numHashes number of hashes in a signature [in]
/// @param alphabetChoice 15 or 10 letters [in]
/// @param badMers Overrepresented KMERS [in]
NCBI_XBLAST_EXPORT
bool minhash_query2(const string& query,
                     vector < vector <uint32_t> >& seq_hash,
                     int kmerNum,
		     int numHashes,
                     int alphabetChoice, 
		     vector<int> badMers,
                     int chunkSize);

NCBI_XBLAST_EXPORT
void get_LSH_match_from_hash(const vector< vector <uint32_t> > &lsh_hash_vec,
                      const uint64_t* lsh_array,
                      vector < set<uint32_t> >& candidates);

NCBI_XBLAST_EXPORT
void get_LSH_hashes(vector < vector <uint32_t> >& query_hash,
                      vector < vector <uint32_t> >& lsh_hash_vec,
                      int num_bands,
                      int rows_per_band);

/// Gets the LSH hash for one hash function.
/// @param query_hash Hash values for query [in]
/// @param lsh_hash_vec LSH query hash [out]
/// @param numHashes number of hashes in signature [in]
/// @param numRows number of rows (2?) in LSH [in]
NCBI_XBLAST_EXPORT
void get_LSH_hashes5(vector < vector <uint32_t> >& query_hash,
                      vector < vector <uint32_t> >&lsh_hash_vec,
                      int numHashes,
                      int numRows);

NCBI_XBLAST_EXPORT
void neighbor_query(const vector < vector <uint32_t> >& query_hash,
                      const uint64_t* lsh,
                      vector< set<uint32_t> >& candidates,
                      CMinHashFile& mhfile,
                      int num_hashes,
                      int min_hits,
                      double thresh,
                      TBlastKmerPrelimScoreVector& score_vector,
                      BlastKmerStats& kmer_stats,
                      int kmerVersion);

/// Get the random numbers for the hash function.
NCBI_XBLAST_EXPORT
void GetRandomNumbers(uint32_t* a, 
		uint32_t* b, 
		int numHashes);

/// Function to get the k sites to compare for Buhler LSH.
NCBI_XBLAST_EXPORT
void GetKValues(vector< vector <int> >& kvector, 
		int k_value, 
		int l_value, 
		int array_size);

// Find candidate matches with LSH.  
// Based on article by Buhler (PMID:11331236) but applied to our arrays
// of hash functions rather than sequences.
//
NCBI_XBLAST_EXPORT
void get_LSH_hashes2(vector < vector <uint32_t> >& query_hash,
                      vector < vector <uint32_t> >&lsh_hash_vec,
                      int num_k,
                      int num_l,
                      vector< vector<int> >& kValues);



NCBI_XBLAST_EXPORT
int BlastKmerVerifyIndex(CRef<CSeqDB> seqdb, string &error_msg);

END_NCBI_SCOPE
#endif /* BLAST_KMER_UTILS_HPP */
