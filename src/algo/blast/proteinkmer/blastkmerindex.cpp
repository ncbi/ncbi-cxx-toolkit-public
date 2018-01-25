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
 *   Build index for BLAST-kmer searches
 *
 * These comments describes the indices used for the KMER (QuickBLASTP) program.
 * These indices are created by the code in this file.
 * This document applies to the version 3 index.  Others are not supported.
 *
 * There are two file extensions:
 * 1.) pki: contain the values for quick matching of the query with database sequence.  After the initial
 * match, the Jaccard distance is calculated using the data in the pkd file.
 * 2.) pkd: contains the data arrays used for Jaccard calculation.
 *
 * Description of pki files:
 * ---------------------------------
 *  1.) version - 4 bytes
 *  2.) number of sequences - 4 bytes
 *  3.) number of hash functions - 4 bytes
 *  4.) number of samples (not used except in version 2) - 4 bytes
 *  5.) KMER size - 4 bytes
 *  6.) Rows per band.  Used for LSH and typically 2 - 4 bytes
 *  7.) compression.  Used for data (pkd file) to say how many bytes per hash value - 4 bytes
 *  8.) Alphabet (size or number?) - 4 bytes
 *  8.) Start of the LSH array - 4 bytes.
 *  9.) Size of the LSH array. Typically 16777217 bytes (2**24 + 1) - 4 bytes
 *  10.) Offset for the end of the LSH matches (not array!) Also, this is the total size of the 
 *  file in bytes. - 8 bytes
 *  11.) Target chunk size. Typically 150. - 4 bytes
 *  12.) Reserved for future use - 4 bytes
 *  13.) Number of bad kmers These are overrepresented kmers - 4 bytes
 *  14.) Hash values for bad kmers.  This area will be filled with zeros if there
 *  are no badmers. - (2*4*number of hash functions-4) bytes
 *  15.) LSH array with each entry (corresponding to hash value for two kmers) 
 *  containing the offset to the list of data arrays in the pkd files. - size of LSH array (given
 *  in item 9 above) * 8 bytes.
 *  16.) An entry at the end of the LSH array with the final offset in bytes. Should be
 *  same as entry 10 and is the size of the pki file. - 8 bytes.
 *  17.) A list of entries for the data arrays in the pkd files - 4 bytes times the number of entries 
 *
 *  Description of pkd files:
 *  ---------------------------------
 *  This file contains the data arrays used in the calculation of the Jaccard distance.
 *  Each array has (number of hash functions) * width.  Width can be 1, 2, or 4 bytes. 
 *  Typically 2 bytes is used.  Note that this compresses the hash values from 4 to 2 bytes, 
 *  so collisions may occur. The number of collisions is small.  At the end of 
 *  each array 4 bytes are reserved for the OID in the BLAST database.  Note that multiple
 *  arrays may refer to the same OID.  Also, duplicate arrays with the same OID are eliminated.
 *  Note that the hash values (before compression) are ordered from largest to smallest. 
 *
 *  Locality Sensitive Hashing (LSH):
 *  ---------------------------------
 *  The idea here is to have some criteria to quickly identify data arrays that are similar to 
 *  the ones in the query and only calculate the Jaccard distance on those arrays.  We do
 *  this by taking two hash values from a data array and producing another hash value from it.
 *  In version 3 index, we have two different ways of producing such LSH hash values.  First, we
 *  take two neighboring hash values (from elements 0 and 1, 1 and 2, 2 and 3, etc.)
 *  and combine them to make one LSH hash value.  Typically, we use 32 hash values in an array,
 *  so this gives us 31 LSH hash values.  Second, we take every other value and hash them to
 *  one LSH hash value (from elements 0 and 2, 1 and 3, etc.)  For 32 hash values in an 
 *  array, this gives us 30 LSH hash values.  
 *
 */

#include <ncbi_pch.hpp>
#include <algo/blast/proteinkmer/blastkmerindex.hpp>
#include <algo/blast/proteinkmer/blastkmerutils.hpp>
#include <algo/blast/proteinkmer/mhfile.hpp>
#include <corelib/ncbifile.hpp>


#ifdef _OPENMP
#include <omp.h>
#endif

#include "pearson.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)



CBlastKmerBuildIndex::CBlastKmerBuildIndex(CRef<CSeqDB> seqdb,
                int kmerSize,
		int numHashFct,
                int samples,
		int compress,
		int alphabet,
		int version,
		int chunkSize)
 : m_NumHashFct(numHashFct),
   m_KmerSize(kmerSize),
   m_SeqDB(seqdb),
   m_DoSeg(0),
   m_Samples(samples),
   m_Compress(compress),
   m_Alphabet(alphabet),
   m_Version(version),
   m_ChunkSize(chunkSize)
{
	if (m_Compress == 0)
		m_Compress=4;

	// Used for version one LSH
	m_RowsPerBand = 2;
	m_NumBands = m_NumHashFct/m_RowsPerBand;

	if (m_Version < 2)
		m_Samples=0; // Occupies the seg position in version 1
}

// universal hashing mod 1048583.
inline uint32_t uhash(uint64_t x, uint64_t a, uint64_t b)
{
                uint32_t p=PKMER_PRIME;
                        return ( (a*x + b) % p );
}


/// FNV Hash.  See http://www.isthe.com/chongo/tech/comp/fnv/index.html
static Uint4 FNV_hash(uint32_t num)
{
     const Uint4 fnv_prime = 16777619u;
     const Uint4 fnv_offset_basis = 2166136261u;
     Int4 i;
     Uint4 hash;

     Uint1 key[4];
     key[0] = num & 0xff;
     key[1] = (num >> 8) & 0xff;
     key[2] = (num >> 16) & 0xff;
     key[3] = (num >> 24) & 0xff;
     

     hash = fnv_offset_basis;
     for (i = 0;i < 4;i++) {
         hash *= fnv_prime;
         hash ^= key[i];
     }

     return hash;
}

// compute kmer set and minhash signature for a query
void s_MinhashSequences(uint32_t q_oid,
				   CSeqDB & db,
				   vector < vector < vector < uint32_t > > > & seq_hash,
				   uint32_t *dead,
				   int num_hashes,
				   const uint32_t *a,
				   const uint32_t *b,
				   bool do_seg,
				   int kmerNum,
				   int oidOffset,
				   int alphabetChoice,
				   int version,
				   int chunkSize)
{
	int fullOID=q_oid+oidOffset; // REAL OID in full db, not just volume.
	int seq_length = db.GetSeqLength(fullOID);
	vector<TSeqRange> range_v;
	int chunk_num = BlastKmerBreakUpSequence(seq_length, range_v, chunkSize);
	seq_hash[q_oid].resize(chunk_num);  // chunk_num is max number that will be saved.

	int chunk_iter=0;
	int chunk_counter=0;
	string query;
	bool first_time=true;
	db.GetSequenceAsString(fullOID, CSeqUtil::e_Ncbistdaa, query);
	for(vector<TSeqRange>::iterator iter=range_v.begin(); iter != range_v.end(); ++iter, chunk_iter++)
	{

		set<uint32_t> seq_kmer = BlastKmerGetKmerSet(query, do_seg, *iter, kmerNum, alphabetChoice);
	
		if (seq_kmer.empty())
		{	
			continue;
		}

	
		vector<uint32_t> idx_tmp(num_hashes);
		vector<uint32_t> hash_tmp(num_hashes);
	
		for(int h=0;h<num_hashes;h++)
		{
			hash_tmp[h]=0xffffffff;
			idx_tmp[h]=0xffffffff;
		}
	
		// for each kmer in the sequence,
		for(set<uint32_t>::iterator i=seq_kmer.begin(); i != seq_kmer.end(); ++i)
		{
			// compute hashes and track their minima
			for(int h=0;h<num_hashes;h++)
			{
				uint32_t hashval = uhash(*i, a[h], b[h]);
			
				if (hashval < hash_tmp[h])
				{
					hash_tmp[h] = hashval;
					idx_tmp[h]  = *i;
				}
			}
		} // end each kmer

		if (first_time == false)
		{
			int distance = BlastKmerGetDistance(idx_tmp, seq_hash[q_oid][chunk_counter]);
			if (distance == 0)
				continue;
			chunk_counter++;  // Last valid chunk iteration
		}
		else
			first_time=false;

	
        	int h;
		// save the kmers with the minimum hash values
		seq_hash[q_oid][chunk_counter].resize(num_hashes+1);
		for(h=0;h<num_hashes;h++)
			seq_hash[q_oid][chunk_counter][h] = idx_tmp[h];

		// Last spot is Query OID
		if (version > 1)
			seq_hash[q_oid][chunk_counter][num_hashes] = q_oid;
		else
			seq_hash[q_oid][chunk_counter][num_hashes] = fullOID;
	}

	///	ERASE the end of the vector if not all chunks filled..
	if (chunk_num > chunk_counter+1)
		seq_hash[q_oid].erase(seq_hash[q_oid].begin()+chunk_counter+1, seq_hash[q_oid].end());
	if (first_time == true)  // NOthing found at all.
		seq_hash[q_oid].erase(seq_hash[q_oid].begin(), seq_hash[q_oid].end());

}

// compute kmer set and minhash signature for a query
void s_MinhashSequences2(uint32_t q_oid,
				   CSeqDB & db,
				   vector < vector < vector < uint32_t > > > & seq_hash,
				   uint32_t *dead,
				   int num_hashes,
				   int kmerNum,
				   int oidOffset,
				   int alphabetChoice,
				   int version,
				   vector<int> badMers,
				   int chunkSize)
{
	int fullOID=q_oid+oidOffset; // REAL OID in full db, not just volume.
	int seq_length = db.GetSeqLength(fullOID);
	vector<TSeqRange> range_v;
	int chunk_num = BlastKmerBreakUpSequence(seq_length, range_v, chunkSize);
	seq_hash[q_oid].resize(chunk_num);  // chunk_num is max number that will be saved.

	int chunk_iter=0;
	int chunk_counter=0;
	string query;
	bool first_time=true;
	db.GetSequenceAsString(fullOID, CSeqUtil::e_Ncbistdaa, query);
	for(vector<TSeqRange>::iterator iter=range_v.begin(); iter != range_v.end(); ++iter, chunk_iter++)
	{

		set<uint32_t> seq_kmer = BlastKmerGetKmerSet2(query, *iter, kmerNum, alphabetChoice, badMers);
	
		if (seq_kmer.empty())
		{	
			continue;
		}

	
		vector<uint32_t> hash_values;
		vector<uint32_t> idx_tmp(num_hashes);
	
		// for each kmer in the sequence,
		for(set<uint32_t>::iterator i=seq_kmer.begin(); i != seq_kmer.end(); ++i)
		{
			uint32_t hashval = FNV_hash(*i);
			hash_values.push_back(hashval);
	
		} // end each kmer
		if (hash_values.size() < static_cast<size_t>(num_hashes))
		{
			int rem = 1 + num_hashes - hash_values.size();
			uint32_t hashval = 0xffffffff;  // Fill in empties
			for (int i=0; i<rem; i++)
				hash_values.push_back(hashval);
		}
		std::sort(hash_values.begin(), hash_values.end());
		
		for(int i=0; i<num_hashes; i++)
			idx_tmp[i] = hash_values[i];

		if (first_time == false)
		{
			int distance = BlastKmerGetDistance(idx_tmp, seq_hash[q_oid][chunk_counter]);
			if (distance == 0)
				continue;
			chunk_counter++;  // Last valid chunk iteration
		}
		else
			first_time=false;

	
        	int h;
		// save the kmers with the minimum hash values
		seq_hash[q_oid][chunk_counter].resize(num_hashes+1);
		for(h=0;h<num_hashes;h++)
			seq_hash[q_oid][chunk_counter][h] = idx_tmp[h];

		// Last spot is Query OID
		if (version > 1)
			seq_hash[q_oid][chunk_counter][num_hashes] = q_oid;
		else
			seq_hash[q_oid][chunk_counter][num_hashes] = fullOID;
	}

	///	ERASE the end of the vector if not all chunks filled..
	if (chunk_num > chunk_counter+1)
		seq_hash[q_oid].erase(seq_hash[q_oid].begin()+chunk_counter+1, seq_hash[q_oid].end());
	if (first_time == true)  // NOthing found at all.
		seq_hash[q_oid].erase(seq_hash[q_oid].begin(), seq_hash[q_oid].end());

}

// LSH via Stanford PDF
static void s_Get_LSH_index_hashes(vector < vector < vector <uint32_t> > >& seq_hash, vector< vector<uint32_t> >& lsh, 
		int q_oid, int numBands, int numRows, int& total_chunks, uint8_t* uniqueHash)
{

	int num_chunks = seq_hash[q_oid].size();
	for (int n=0; n<num_chunks; n++)
	{
	  for(int b=0;b<numBands;b++)
	  {
		unsigned char key[9];
		for(int r=0;r<numRows;r++)
		{
			key[r*4] = (seq_hash[q_oid][n][b*numRows+r]) & 0xff;
			key[1+r*4] = ((seq_hash[q_oid][n][b*numRows+r]) >> 8) & 0xff;
			key[2+r*4] = ((seq_hash[q_oid][n][b*numRows+r]) >> 16) & 0xff;
			key[3+r*4] = ((seq_hash[q_oid][n][b*numRows+r]) >> 24) & 0xff;
		}
		key[8] = (unsigned char) b;
		uint32_t foo = do_pearson_hash(key, 9);
		uniqueHash[foo] = 1;
		if (foo != 0)
			lsh[foo].push_back(total_chunks);
	  }
	  total_chunks++;
	}
}

// LSH via Buhler
static void s_Get_LSH_index_hashes2(vector < vector < vector <uint32_t> > >& seq_hash, vector< vector<uint32_t> >& lsh,
			int q_oid, int num_k, int num_l, int array_size, int& total_chunks, uint8_t* uniqueHash,
			vector < vector <int> >& kvector)
{

	int max=4*num_k+1;
        vector<unsigned char> key(max);
	int num_chunks=seq_hash[q_oid].size();
	int temp_index=0;
	int temp_hash=0;
	for (int n=0; n<num_chunks; n++)
	{
		for (int r=0; r<num_l; r++)
        {
            for (int i=0; i<num_k; i++)
            {
                    temp_index = kvector[r][i];
                    temp_hash = seq_hash[q_oid][n][temp_index];
                    key[i*4] = (temp_hash) & 0xff;
                    key[1+i*4] = ((temp_hash) >> 8) & 0xff;
                    key[2+i*4] = ((temp_hash) >> 16) & 0xff;
                    key[3+i*4] = ((temp_hash) >> 24) & 0xff;
            }
            key[max-1] = r;
            uint32_t foo = do_pearson_hash(key.data(), max);
            uniqueHash[foo] = 1;
            if (foo != 0)
                lsh[foo].push_back(total_chunks);
        }
        total_chunks++;
	}
}

// One hash function LSH ala Stanford, but add extra rows
static void s_Get_LSH_index_hashes5(vector < vector < vector <uint32_t> > >& seq_hash, vector< vector<uint32_t> >& lsh, 
		int q_oid, int numHashes, int numRows, int& total_chunks, uint8_t* uniqueHash)
{

	int num_chunks = seq_hash[q_oid].size();
	int numHashMax = numHashes - numRows + 1;
	uint32_t temp_hash = 0;
	for (int n=0; n<num_chunks; n++)
	{
	  for(int b=0;b<numHashMax;b++)
	  {
		unsigned char key[12]; // 12 assumes numRows is 3 or less.
		for(int r=0;r<numRows;r++)
		{
			key[r*4] = (seq_hash[q_oid][n][b+r]) & 0xff;
			key[1+r*4] = ((seq_hash[q_oid][n][b+r]) >> 8) & 0xff;
			key[2+r*4] = ((seq_hash[q_oid][n][b+r]) >> 16) & 0xff;
			key[3+r*4] = ((seq_hash[q_oid][n][b+r]) >> 24) & 0xff;
		}
		uint32_t foo = do_pearson_hash(key, 4*numRows);
		uniqueHash[foo] = 1;
		if (foo != 0)
			lsh[foo].push_back(total_chunks);
	  }
	  for(int b=0;b<numHashMax-1; b++)
	  {
		unsigned char key[8]; // 8 assumes 2 or less.
		for (int r=0; r<numRows; r++)
		{
			temp_hash = seq_hash[q_oid][n][b+2*r];
			key[r*4] = (temp_hash) & 0xff;
			key[1+r*4] = (temp_hash >> 8) & 0xff;
			key[2+r*4] = (temp_hash >> 16) & 0xff;
			key[3+r*4] = (temp_hash >> 24) & 0xff;
		}
		uint32_t foo = do_pearson_hash(key, 4*numRows);
		uniqueHash[foo] = 1;
		if (foo != 0)
			lsh[foo].push_back(total_chunks);
	  }

	  total_chunks++;
	}
}

void
CBlastKmerBuildIndex::Build(int numThreads)
{
	m_SeqDB->SetNumberOfThreads(numThreads);

	vector<string> paths;
	int oid_offset=0;
	CSeqDB::FindVolumePaths(m_SeqDB->GetDBNameList(), CSeqDB::eProtein, paths, NULL, false);
	vector<TSeqRange> range_vec;
	vector<string> volname_vec;
	int last=0;
	for (vector<string>::iterator iter=paths.begin(); iter != paths.end(); ++iter)
	{
		string base;
		string ext;
		CDirEntry::SplitPath((*iter), NULL, &base, &ext);
		string volName = base + ext;
		volname_vec.push_back(volName);	
		CRef<CSeqDB> seqdb_vol(new CSeqDB(*iter, CSeqDB::eProtein));
		oid_offset += seqdb_vol->GetNumSeqs();
		TSeqRange range;
               	range.SetFrom(last);
               	range.SetTo(oid_offset);
		range_vec.push_back(range);
		last=oid_offset;
	}

	int numVols = paths.size();
#pragma omp parallel for num_threads(numThreads)
	for(int index=0; index<numVols; index++)
	{
		x_BuildIndex(volname_vec[index], range_vec[index].GetFrom(), range_vec[index].GetTo());
	}
}


vector<int>
s_BlastKmerLoadBadMers(int alphabet)
{
	char* loadBadMers = getenv("LOADBADMERS");
	if (loadBadMers)
	{
        	CNcbiIfstream in("badMers.in");
        	int badKmer;
		vector<int> badMers;
        	while (in)
        	{
               	 in >> badKmer;
               	 if (! in)
                        break;
               	 badMers.push_back(badKmer);
cerr << badKmer << '\n';
		}
		return badMers;
	}
	char* noBadMers = getenv("NOBADMERS");
	if (noBadMers)
		return vector<int>();

	if (alphabet == 1)
	{
		const int kLength=10;
		int array[] = {139810, 69905, 70161, 70177, 74257, 
			69921, 69906, 74001, 135441, 69922};
		vector<int> badMers(array, array+kLength);
		return badMers;
	}

	return vector<int>();
}

void
CBlastKmerBuildIndex::x_BuildIndex(string& name, int start, int stop)
{
	int StartLSH=0;
	if (m_Version == 2)
	{
		int vectorRandNums=0;
		vectorRandNums = m_Samples*m_RowsPerBand;
		vectorRandNums += 8 - (m_Samples*m_RowsPerBand)%8;  // multiple of 8.
		StartLSH=56+(2*4*m_NumHashFct)+vectorRandNums;  
	}
	else if (m_Version == 3)
		StartLSH=56+(2*4*m_NumHashFct);  
	else
		StartLSH=48+(2*4*m_NumHashFct);  
	const int kSizeLSH=KMER_LSH_ARRAY_SIZE; // 2**24 + 1 (sentinel at end)

	string indexFile = name + ".pki";
	string dataFile = name + ".pkd";
	// Output files 
    CNcbiOfstream index_file(indexFile.c_str(), IOS_BASE::out | IOS_BASE::binary);
    CNcbiOfstream data_file(dataFile.c_str(), IOS_BASE::out | IOS_BASE::binary);

	if (!index_file)
		 NCBI_THROW(CFileException, eNotExists, "Cannot open " + indexFile);
	if (!data_file)
		 NCBI_THROW(CFileException, eNotExists, "Cannot open " + dataFile);

	int num_seqs = 0;
	if (stop != 0)
		num_seqs = stop - start; 
	else
		num_seqs = m_SeqDB->GetNumSeqs();


	index_file.write((char *) &(m_Version), 4);  // Add BLAST DB name?
	index_file.write((char *) &(num_seqs), 4);  
	index_file.write((char *) &(m_NumHashFct), 4);
	index_file.write((char *) &(m_Samples), 4);
	index_file.write((char *) &(m_KmerSize), 4);
	index_file.write((char *) &(m_RowsPerBand), 4);
	index_file.write((char *) &(m_Compress), 4);
	index_file.write((char *) &(m_Alphabet), 4);
	index_file.write((char *) &(StartLSH), 4);
	index_file.write((char *) &(kSizeLSH), 4);
	
    // hash coefficients
    vector<uint32_t> a(m_NumHashFct);
    vector<uint32_t> b(m_NumHashFct);
    GetRandomNumbers(a.data(), b.data(), m_NumHashFct);
        
	// sequences that contain no valid kmers
    uint32_t* dead = new uint32_t[3*num_seqs];
    _ASSERT(dead);
	for(int q_oid=0;q_oid<3*num_seqs;q_oid++)
		dead[q_oid]=0;

    vector<int> badMers = s_BlastKmerLoadBadMers(m_Alphabet);
	
	// mapping from a sequence (by oid) to 
	// chunks of a sequence to the minhashes
	vector < vector < vector < uint32_t > > > seq_hash(num_seqs);
	
	// compute minhash signatures for each sequence
	for(int q_oid=0;q_oid<num_seqs;q_oid++)
	{
		if (m_Version < 3)
			s_MinhashSequences(q_oid,
					  *m_SeqDB,
					  seq_hash,
					  dead,
					  m_NumHashFct,
					  a.data(),
					  b.data(),
					  m_DoSeg,
					  m_KmerSize,
					  start,
					  m_Alphabet,
					  m_Version,
					  m_ChunkSize); 
		else
			s_MinhashSequences2(q_oid,
					  *m_SeqDB,
					  seq_hash,
					  dead,
					  m_NumHashFct,
					  m_KmerSize,
					  start,
					  m_Alphabet,
					  m_Version,
					  badMers,
					  m_ChunkSize); 
	}
	
	const uint32_t kUniqueHash = 0x1000000;
        uint8_t* uniqueHash = new uint8_t[kUniqueHash];
	for (uint32_t index=0; index<kUniqueHash; index++)
		uniqueHash[index] = 0;

	vector< vector<uint32_t> > lsh(kSizeLSH);
	int total_chunks=0;
// cerr << "TEST " << m_RowsPerBand << " " << m_Samples << '\n';
	vector < vector <int> > kvector;
 	if (m_Version == 2) 
 	{
		GetKValues(kvector, m_RowsPerBand, m_Samples, m_NumHashFct);
	}
	for(int q_oid=0;q_oid<num_seqs;q_oid++)
	{
		if (dead[q_oid])  // Nothing to do with this.
			continue;
			
		if (m_Version == 2) 
			s_Get_LSH_index_hashes2(seq_hash, lsh, q_oid, m_RowsPerBand, m_Samples, m_NumHashFct, 
				total_chunks, uniqueHash, kvector);
		else if (m_Version == 3)
			s_Get_LSH_index_hashes5(seq_hash, lsh, q_oid, m_NumHashFct, m_RowsPerBand, total_chunks, uniqueHash);
		else 
			s_Get_LSH_index_hashes(seq_hash, lsh, q_oid, m_NumBands, m_RowsPerBand, total_chunks, uniqueHash);
	}

	int hashCount=0;
        for(uint32_t index=0; index<kUniqueHash; index++)
        {
                if (uniqueHash[index] > 0)
                        hashCount++;
        }
	delete [] uniqueHash;


	uint64_t LSHMatchSize=0;
        for(int index=0; index<kSizeLSH-1; index++)
	{
		LSHMatchSize += lsh[index].size();
	}

	// End of the LSH matches (NOT the LSH array)
	const uint64_t kLSHMatchEnd = 4*LSHMatchSize + StartLSH + 8*kSizeLSH;
	index_file.write((char *) &(kLSHMatchEnd), 8);

	if (m_Version > 2)
		index_file.write((char *) &(m_ChunkSize), 4);
	
	if (m_Version > 1)
	{
		const int kFutureUse=0;
		index_file.write((char *) &(kFutureUse), 4);
		if (m_Version < 3)
			index_file.write((char *) &(kFutureUse), 4);
	}

	if (m_Version < 3)
	{
		// Random numbers used for the hash functions.
		for(int i=0;i<m_NumHashFct;i++)
			index_file.write((char *) &(a[i]), 4);
		for(int i=0;i<m_NumHashFct;i++)
			index_file.write((char *) &(b[i]), 4);
	}
	else
	{
		int i=1; // i must be lower than 2*m_NumHashFct
		int num=badMers.size();
		if (num > (2*m_NumHashFct-1))
			num = 2*m_NumHashFct-1;
		index_file.write((char *) &(num), 4);
		for(vector<int>::iterator iter=badMers.begin(); iter != badMers.end() && i<2*m_NumHashFct; ++iter, ++i)
			index_file.write((char *) &(*iter), 4);
		if (i<2*m_NumHashFct)
		{
			const int kZero=0;
			for (; i<2*m_NumHashFct; ++i)
				index_file.write((char *) &(kZero), 4);
		}
		
	}

	
	if (m_Version == 2)
	{
		for (int i=0; i<m_Samples; i++)
		{
			vector<int> temp = kvector[i];
			for (int j=0; j<m_RowsPerBand; j++)  // values <= number of hash functions.  
				index_file.write((char *) &(temp[j]), 1);
		}
		// Need to have multiple of 8 bytes.
		int extra = 8 - (m_Samples*m_RowsPerBand)%8;
		Uint1 temp=0;
		for (int i=0; i<extra; i++)
			index_file.write((char *) &(temp), 1);
	}


	uint64_t lsh_offset = StartLSH + 8*kSizeLSH;
	const uint64_t kNoValue = 0;  // StartLSH is greater than zero, so this is OK.
        for(int index=0; index<kSizeLSH-1; index++)
	{
		if (lsh[index].size() == 0)
			index_file.write((char *) &(kNoValue), 8);
		else	
			index_file.write((char *) &(lsh_offset), 8);
		lsh_offset += 4*(lsh[index].size());
	}
	index_file.write((char *) &(lsh_offset), 8);  // Last one, sentinel

	int lsh_hits=0;
        for(int index=0; index<kSizeLSH-1; index++)
	{
		for(vector<uint32_t>::iterator i=lsh[index].begin(); i != lsh[index].end(); ++i)
		{
			index_file.write((char *) &(*i), 4);
			lsh_hits++;
		}
	}
	
	index_file.flush();
	index_file.close();
	
	x_WriteDataFile(seq_hash, num_seqs, data_file);
	data_file.flush();
	data_file.close();
	

        delete [] dead;

	return;
}	


void
CBlastKmerBuildIndex::x_WriteDataFile(vector < vector < vector < uint32_t > > > & seq_hash, int num_seqs, CNcbiOfstream& data_file)
{
	int width = m_Compress;
	if (width == 0) 
		width = 4;
	for(int q_oid=0;q_oid<num_seqs;q_oid++)
	{
		int num_chunks = seq_hash[q_oid].size();
		for (int n=0; n<num_chunks; n++)
		{
		    vector<uint32_t> tmp_hash;
		    for(int b=0;b<m_NumHashFct;b++)
		    {
			if (width == 2)
			{
				uint16_t hash_val = pearson_hash_int2short(seq_hash[q_oid][n][b]);
				tmp_hash.push_back(hash_val);
				// data_file.write((char *) &(hash_val), width);
			}
			else if(width == 1)
			{
				unsigned char hash_val = pearson_hash_int2byte(seq_hash[q_oid][n][b]);
				tmp_hash.push_back(hash_val);
				// data_file.write((char *) &(hash_val), width);
			}
			else
			{
				// data_file.write((char *) &(seq_hash[q_oid][n][b]), width);
				tmp_hash.push_back(seq_hash[q_oid][n][b]);
			}
		    }
		    if (m_Version == 3)
			std::sort(tmp_hash.begin(), tmp_hash.end());
		    for(int b=0;b<m_NumHashFct;b++)
		    {
		    	data_file.write((char *) &(tmp_hash[b]), width);  
		    }
		    data_file.write((char *) &(seq_hash[q_oid][n][m_NumHashFct]), 4);  // OID in the last slot
		}
	}
}

END_SCOPE(blast)
END_NCBI_SCOPE

