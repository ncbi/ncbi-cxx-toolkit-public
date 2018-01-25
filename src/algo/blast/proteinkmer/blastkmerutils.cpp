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
 *   Utilities for the BLAST kmer search.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_seg.h>
#include <algo/blast/core/blast_encoding.h>
#include <math.h>

#include <algo/blast/proteinkmer/blastkmerutils.hpp>
#include <algo/blast/proteinkmer/mhfile.hpp>

#include <util/random_gen.hpp>

#include "pearson.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

DEFINE_STATIC_MUTEX(randMutex);


// universal hashing mod 1048583.
inline uint32_t uhash(uint64_t x, uint64_t a, uint64_t b)
{
              uint32_t p=PKMER_PRIME;
              return ( (a*x + b) % p );
}


/// FNV hash, see http://www.isthe.com/chongo/tech/comp/fnv/index.html
static uint32_t FNV_hash(uint32_t num)
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


// estimate Jaccard similarity using minhashes
inline double estimate_jaccard(vector<uint32_t>& query_hash, vector<uint32_t>& subject, int num_hashes)
{
	int score=0;
	
	// help the compiler vectorize
	uint32_t *a=&(query_hash[0]);
	uint32_t *b=&(subject[0]);
	
	for(int h=0;h<num_hashes;h++)
	{
		if (a[h] == b[h])
			score++;
	}
	
	return (double) score / num_hashes;
}


// estimate Jaccard similarity with one hash function
inline double estimate_jaccard2(vector<uint32_t>& query_hash, vector<uint32_t>& subject, int num_hashes)
{
	int score=0;
	
	// help the compiler vectorize
	uint32_t *a=&(query_hash[0]);
	uint32_t *b=&(subject[0]);

	int bindex=0;
	for(int h=0;h<num_hashes;h++)
	{	// DOes a[h] < b[bindex] make any sense?
		while (bindex < num_hashes && a[h] > b[bindex])
		{
			bindex++;
		}
		if (bindex == num_hashes)
			break;
		if (a[h] == b[bindex])
			score++;
	}
	
	return (double) score / num_hashes;
}

set<uint32_t> BlastKmerGetKmerSetStats(const string& query_sequence, int kmerNum, map<string, int>& kmerCount, map<string, int>& kmerCountPlus, int alphabetChoice, bool perQuery)
{
	// the set of unique kmers
	set<uint32_t> kmer_set;

        vector<Uint1> trans_table;
        BlastKmerGetCompressedTranslationTable(trans_table, alphabetChoice);
	
	int seq_length = query_sequence.length();
	const char* query = query_sequence.c_str();

	// bail out if the sequence is too short
	if (seq_length < kmerNum)
		return kmer_set;

	map<string, int> kmerCountPriv;
	map<string, int> kmerCountPlusPriv;

	for(int i=0;i<seq_length-kmerNum+1;i++)
	{

		// compute its index
		uint32_t index=0;
		for (int kindex=0; kindex<kmerNum; kindex++)
		{       // Check for X in the kmer
			if (query[i+kindex] == 21)
			{
				index=0;
				break;
			}
			index = (index << 4);
			index += trans_table[query[i+kindex]];
		}
		if (index != 0)
                        kmer_set.insert(index);
		if (index != 0)
		{
			string kmer_str="";
			kmer_str += NStr::NumericToString(index) + " ";
/*
			for (int myindex=0; myindex<kmerNum; myindex++)
				kmer_str += NStr::NumericToString((int)query[i+myindex]) + " ";
*/
			kmerCountPriv[kmer_str]++;
		}
		if (index != 0 && i<seq_length-kmerNum && query[i+kmerNum] != 21)
		{
			int index_short = index;
			index = (index << 4);
			index += trans_table[query[i+kmerNum]];
			string kmer_str="";
			kmer_str += NStr::NumericToString(index) + " " + NStr::NumericToString(index_short);
			kmerCountPlusPriv[kmer_str]++;
		}
	}

	if (perQuery)
	{
		for (map<string, int>::iterator i=kmerCountPriv.begin(); i != kmerCountPriv.end(); ++i)
                	kmerCount[(*i).first]++;
		for (map<string, int>::iterator i=kmerCountPlusPriv.begin(); i != kmerCountPlusPriv.end(); ++i)
                	kmerCountPlus[(*i).first]++;
	}
	else
	{
		for (map<string, int>::iterator i=kmerCountPriv.begin(); i != kmerCountPriv.end(); ++i)
                	kmerCount[(*i).first] += (*i).second;
		for (map<string, int>::iterator i=kmerCountPlusPriv.begin(); i != kmerCountPlusPriv.end(); ++i)
                	kmerCountPlus[(*i).first] += (*i).second;
	}
	
	return kmer_set;
}

set<uint32_t> BlastKmerGetKmerSet(const string& query_sequence, bool do_seg, TSeqRange& range, int kmerNum, int alphabetChoice)
{
	// the set of unique kmers
	set<uint32_t> kmer_set;

        vector<Uint1> trans_table;
        BlastKmerGetCompressedTranslationTable(trans_table, alphabetChoice);
	
	int seq_length = query_sequence.length();
	const char* query = query_sequence.c_str();

	// bail out if the sequence is too short
	if (seq_length < kmerNum)
		return kmer_set;

	int chunk_length=range.GetLength();
	char *query_private=(char*)malloc(chunk_length);
	int private_index=0;
	for (unsigned int i=range.GetFrom(); i<=range.GetTo(); i++, private_index++)
		query_private[private_index] = query[i];

	if (do_seg)
	{
		// filter the query to remove overrepresented regions
		SegParameters *sp = SegParametersNewAa();
		BlastSeqLoc* seq_locs = NULL;
		SeqBufferSeg((unsigned char *)query_private, chunk_length, 0, sp, &seq_locs);
		SegParametersFree(sp);
	
		// mask out low complexity regions with X residues
		for(BlastSeqLoc *itr = seq_locs; itr; itr = itr->next)
		{
			for(int i=itr->ssr->left; i <= itr->ssr->right; i++)
				query_private[i]=21;
		}
	
		BlastSeqLocFree(seq_locs);
	}
	
	for(int i=0;i<chunk_length-kmerNum+1;i++)
	{

		// compute its index
		uint32_t index=0;
		for (int kindex=0; kindex<kmerNum; kindex++)
		{       // Check for X in the kmer
			if (query_private[i+kindex] == 21)
			{
				index=0;
				break;
			}
			index = (index << 4);
			index += trans_table[query_private[i+kindex]];
		}
		if (index != 0)
                        kmer_set.insert(index);
	}
	
	// free the sequence
	free(query_private);
	
	return kmer_set;
}

set<uint32_t> BlastKmerGetKmerSet2(const string& query_sequence, TSeqRange& range, int kmerNum, int alphabetChoice, vector<int> badMers)
{
	// the set of unique kmers
	set<uint32_t> kmer_set;

        vector<Uint1> trans_table;
        BlastKmerGetCompressedTranslationTable(trans_table, alphabetChoice);
	
	int seq_length = query_sequence.length();
	const char* query = query_sequence.c_str();

	// bail out if the sequence is too short
	if (seq_length < kmerNum)
		return kmer_set;

	int chunk_length=range.GetLength();
	char *query_private=(char*)malloc(chunk_length);
	int private_index=0;
	for (unsigned int i=range.GetFrom(); i<=range.GetTo(); i++, private_index++)
		query_private[private_index] = query[i];

	for(int i=0;i<chunk_length-kmerNum+1;i++)
	{

		// compute its index
		uint32_t index=0;
		for (int kindex=0; kindex<kmerNum; kindex++)
		{       // Check for X in the kmer
			if (query_private[i+kindex] == 21)
			{
				index=0;
				break;
			}
			index = (index << 4);
			index += trans_table[query_private[i+kindex]];
		}
		if (index != 0)
		{
			if (i < chunk_length-kmerNum && !badMers.empty())
			{
				std::vector<int>::iterator it;
				it = std::find(badMers.begin(), badMers.end(), index);
				if (it != badMers.end() && i < chunk_length-1)
                        	{
                               		index = (index << 4);
                                	index += trans_table[query_private[i+kmerNum]];
                        	}
			}
                        kmer_set.insert(index);
		}
	}
	
	// free the sequence
	free(query_private);
	
	return kmer_set;
}



void BlastKmerGetCompressedTranslationTable(vector<Uint1>& trans_table, int alphabetChoice)
{
	const unsigned int kAlphabetLen = 28;
        // Compressed alphabets taken from 
        // Shiryev et al.(2007),  Bioinformatics, 23:2949-2951
        const char* kCompAlphabets[] = {
            // 23-to-15 letter compressed alphabet. Based on SE_B(14) 
            "ST IJV LM KR EQZ A G BD P N F Y H C W",
            // 23-to-10 letter compressed alphabet. Based on SE-V(10)
            "IJLMV AST BDENZ KQR G FY P H C W"
        };

	// Use second alphabet only
        const char* trans_string = kCompAlphabets[alphabetChoice];

        Uint4 compressed_letter = 1; // this allows for gaps
        trans_table.clear();
        trans_table.resize(kAlphabetLen + 1, 0);
        for (Uint4 i = 0; i < strlen(trans_string);i++) {
            if (isspace(trans_string[i])) {
                compressed_letter++;
            }
            else if (isalpha(trans_string[i])) {
                Uint1 aa_letter = AMINOACID_TO_NCBISTDAA[(int)trans_string[i]];

                _ASSERT(aa_letter < trans_table.size());

                trans_table[aa_letter] = compressed_letter;
            }
        }
}

int BlastKmerBreakUpSequence(int length, vector<TSeqRange>& range_v, int ChunkSize)
{
	int numChunks=0;
        int newChunkSize=0;
	const int kOverlap=50;
	// Adjust chunk size so chunks are all about same length.
        if(length <= ChunkSize)
	{
		numChunks=1;
		newChunkSize=length;
	} else {
		if (ChunkSize <= kOverlap)
			numChunks=1;
		else
			numChunks = MAX(1, (length - kOverlap)/(ChunkSize - kOverlap));
        	newChunkSize = (length + (numChunks-1)*kOverlap)/numChunks;
		if(newChunkSize > 1.1*ChunkSize)
		{  
			numChunks++;
        		newChunkSize = (length + (numChunks-1)*kOverlap)/numChunks;
		}
	}
	ChunkSize=newChunkSize;
	TSeqPos last=0;
	for (int i=0; i<numChunks; i++)
	{
		TSeqPos end = last + ChunkSize;
		end = MIN(end, (TSeqPos)(length-1));
		TSeqRange range;
		range.SetFrom(last);
		range.SetTo(end);
		range_v.push_back(range);
		last = last + ChunkSize - kOverlap;
	}
	return numChunks;
}

int BlastKmerGetDistance(const vector<uint32_t>& minhash1, const vector<uint32_t>& minhash2)
{

        int distance=0;
	int num_hashes = minhash1.size();

        for (int index=0; index<num_hashes; index++)
        {
                if (minhash1[index] != minhash2[index])
                           distance++;
        }

	return distance;
}

// same as above without database
bool minhash_query(const string& query,
                     vector < vector <uint32_t> >& seq_hash,
                     int num_hashes,
                     uint32_t *a,
                     uint32_t *b,
		     int do_seg,
		     int kmerNum,
                     int alphabetChoice,
                     int chunkSize)
{
	bool kmersFound=false; // return value;
	int seq_length = query.length();
	vector<TSeqRange> range_v;
	int chunk_num = BlastKmerBreakUpSequence(seq_length, range_v, chunkSize);
	seq_hash.resize(chunk_num);
	bool seg = (do_seg > 0) ? true : false;

	vector<uint32_t> idx_tmp(num_hashes);
	vector<uint32_t> hash_tmp(num_hashes);
	int chunk_iter=0;
    for(vector<TSeqRange>::iterator iter=range_v.begin(); iter != range_v.end(); ++iter, chunk_iter++)
    {
	
		seq_hash[chunk_iter].resize(num_hashes);

		set<uint32_t> seq_kmer = BlastKmerGetKmerSet(query, seg, *iter, kmerNum, alphabetChoice);
	
		if (seq_kmer.empty())
			continue;
	
		kmersFound = true;
	
#pragma ivdep
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
	
		// save the kmers with the minimum hash values
		for(int h=0;h<num_hashes;h++)
		{
			seq_hash[chunk_iter][h] = idx_tmp[h];
		}
	}
	return kmersFound;
}

// Hash of kmers by one hash function
bool minhash_query2(const string& query,
                     vector < vector <uint32_t> >& seq_hash,
		     int kmerNum,
		     int numHashes,
                     int alphabetChoice, 
		     vector<int> badMers,
                     int chunkSize)
{
	bool kmersFound=false; // return value;
	int seq_length = query.length();
	vector<TSeqRange> range_v;
	int chunk_num = BlastKmerBreakUpSequence(seq_length, range_v, chunkSize);
	seq_hash.resize(chunk_num);

	vector<uint32_t> hash_values;
	int chunk_iter=0;
    for(vector<TSeqRange>::iterator iter=range_v.begin(); iter != range_v.end(); ++iter, chunk_iter++)
    {
		hash_values.clear();
		seq_hash[chunk_iter].resize(numHashes);

		set<uint32_t> seq_kmer = BlastKmerGetKmerSet2(query, *iter, kmerNum, alphabetChoice, badMers);
	
		if (seq_kmer.empty())
			continue;
	
		kmersFound = true;
	
		// for each kmer in the sequence,
		for(set<uint32_t>::iterator i=seq_kmer.begin(); i != seq_kmer.end(); ++i)
		{
			uint32_t hashval = FNV_hash(*i);
			hash_values.push_back(hashval);
				
		} // end each kmer
		
		if (hash_values.size() < static_cast<size_t>(numHashes))
        {
            int rem = 1 + numHashes - hash_values.size();
            uint32_t hashval = 0xffffffff;  // Fill in empties
            for (int i=0; i<rem; i++)
                    hash_values.push_back(hashval);
        }
        std::sort(hash_values.begin(), hash_values.end());

		// save the kmers with the minimum hash values
		for(int h=0;h<numHashes;h++)
			seq_hash[chunk_iter][h] = hash_values[h];
	}
	return kmersFound;
}

// Find candidate matches with LSH (based on array in index)
void get_LSH_hashes(vector < vector <uint32_t> >& query_hash,
		      vector < vector <uint32_t> >&lsh_hash_vec,
                      int num_bands,
                      int rows_per_band)
{
	int num_chunks=query_hash.size();
	uint32_t temp_hash=0;
	for (int n=0; n<num_chunks; n++)
	{
		vector <uint32_t> lsh_chunk_vec; 
		for(int b=0;b<num_bands;b++)
		{
		
			// LSH
			unsigned char key[9];
			for(int r=0;r<rows_per_band;r++)
			{
				temp_hash = query_hash[n][b*rows_per_band+r];
				key[r*4] = (temp_hash) & 0xff;
               	         	key[1+r*4] = ((temp_hash) >> 8) & 0xff;
               	         	key[2+r*4] = ((temp_hash) >> 16) & 0xff;
               	         	key[3+r*4] = ((temp_hash) >> 24) & 0xff;
			}
			key[8] = (unsigned char) b;
			uint32_t foo = do_pearson_hash(key, 9);
			lsh_chunk_vec.push_back(foo);
		}
		std::sort(lsh_chunk_vec.begin(), lsh_chunk_vec.end());
		lsh_hash_vec.push_back(lsh_chunk_vec);
	}
	
	return;
}

void get_LSH_hashes5(vector < vector <uint32_t> >& query_hash,
		      vector < vector <uint32_t> >&lsh_hash_vec,
                      int numHashes,
                      int numRows)
{
	int num_chunks=query_hash.size();
	uint32_t temp_hash=0;
	int numHashMax = numHashes - numRows + 1;
	for (int n=0; n<num_chunks; n++)
	{
		vector <uint32_t> lsh_chunk_vec; 
		for(int b=0;b<numHashMax;b++)
		{
			// LSH
			unsigned char key[8];
			for(int r=0;r<numRows;r++)
			{
				temp_hash = query_hash[n][b+r];
				key[r*4] = (temp_hash) & 0xff;
               	         	key[1+r*4] = ((temp_hash) >> 8) & 0xff;
               	         	key[2+r*4] = ((temp_hash) >> 16) & 0xff;
               	         	key[3+r*4] = ((temp_hash) >> 24) & 0xff;
			}
			uint32_t foo = do_pearson_hash(key, 4*numRows);
			lsh_chunk_vec.push_back(foo);
		}
		for(int b=0;b<numHashMax-1;b++)
		{
			unsigned char key[8];
			for(int r=0;r<numRows;r++)
			{
				temp_hash = query_hash[n][b+2*r];
				key[r*4] = (temp_hash) & 0xff;
               	         	key[1+r*4] = ((temp_hash) >> 8) & 0xff;
               	         	key[2+r*4] = ((temp_hash) >> 16) & 0xff;
               	         	key[3+r*4] = ((temp_hash) >> 24) & 0xff;
			}
			uint32_t foo = do_pearson_hash(key, 4*numRows);
			lsh_chunk_vec.push_back(foo);
		}
		std::sort(lsh_chunk_vec.begin(), lsh_chunk_vec.end());
		lsh_hash_vec.push_back(lsh_chunk_vec);
	}

	return;
}

void GetRandomNumbers(uint32_t* a, uint32_t* b, int numHashes)
{
	CMutexGuard guard(randMutex);
        CRandom random(1);  // Always have the same random numbers.
        for(int i=0;i<numHashes;i++)
        {
                do
                {
                        a[i]=(random.GetRand()%PKMER_PRIME);
                }
                while (a[i] == 0);

                b[i]=(random.GetRand()%PKMER_PRIME);
        }
}

void GetKValues(vector< vector <int> >& kvector, int k_value, int l_value, int array_size)
{
	CMutexGuard guard(randMutex);
	CRandom random(10);
	
	for (int i=0; i<l_value; i++)
	{
		vector<int> ktemp;
		for (int k=0; k<k_value; k++)
		{
			Uint1 temp=(Uint1) random.GetRand()%array_size;
			ktemp.push_back(temp);
		}
		kvector.push_back(ktemp);
	}
	return;
}


// Find candidate matches with LSH.  
// Based on article by Buhler (PMID:11331236) but applied to our arrays
// of hash functions rather than sequences.
void get_LSH_hashes2(vector < vector <uint32_t> >& query_hash,
		      vector < vector <uint32_t> >& lsh_hash_vec,
                      int num_k,
                      int num_l,
			vector< vector<int> >& kvector)
{
	int max=4*num_k+1;
	vector<unsigned char> key(max);
	int num_chunks=query_hash.size();
	uint32_t temp_hash=0;
	int temp_index=0;
	for (int n=0; n<num_chunks; n++)
	{
		vector <uint32_t> lsh_chunk_vec; 
		for (int r=0; r<num_l; r++)
		{
			for (int i=0; i<num_k; i++)
			{
				temp_index=kvector[r][i];
				temp_hash = query_hash[n][temp_index];
				key[i*4] = (temp_hash) & 0xff;
               		        key[1+i*4] = ((temp_hash) >> 8) & 0xff;
               		        key[2+i*4] = ((temp_hash) >> 16) & 0xff;
               		        key[3+i*4] = ((temp_hash) >> 24) & 0xff;
			}
			key[max-1] = r;
			uint32_t foo = do_pearson_hash(key.data(), max);
			lsh_chunk_vec.push_back(foo);
		}
		std::sort(lsh_chunk_vec.begin(), lsh_chunk_vec.end());
		lsh_hash_vec.push_back(lsh_chunk_vec);
	}
}

// Find candidate matches with LSH (based on array in index)
void get_LSH_match_from_hash(const vector< vector <uint32_t> >& query_LSH_hash,
                     const uint64_t* lsh_array,
		     vector< set<uint32_t > >& candidates)
{

	int num=query_LSH_hash.size();
	for (int i=0; i<num; i++)
	{
        	for(vector<uint32_t>::const_iterator iter=query_LSH_hash[i].begin(); iter != query_LSH_hash[i].end(); ++iter)
		{
			if (lsh_array[*iter] != 0)
				candidates[i].insert(*iter);
		}
	}
	
	return;
}
		     

void
s_HashHashQuery(const vector < vector <uint32_t> > & query_hash, vector < vector <uint32_t> >& query_hash_hash, int compress, int version)
{
    int hash_value=0;
    int num_chunks=query_hash.size();
    const uint32_t kBig=0xffffffff;
    for (int n=0; n<num_chunks; n++)
    {
        vector<uint32_t> tmp_hash;
        for(vector<uint32_t>::const_iterator i = query_hash[n].begin(); i != query_hash[n].end(); ++i)
        {
            if (compress == 2)
                hash_value = (int) pearson_hash_int2short(*i);
            else if (compress == 1)
                hash_value = (int) pearson_hash_int2byte(*i);
            else
                hash_value = *i;

            if (version == 3 && *i == kBig)
            {
                if (compress == 2)
                    tmp_hash.push_back(0xffff);
                else if (compress == 1)
                    tmp_hash.push_back(0xff);
                else
                    tmp_hash.push_back(kBig);
            }
            else
                tmp_hash.push_back(hash_value);
        }
        if (version == 3)  // kBig et al gets sorted to the end anyway.
            std::sort(tmp_hash.begin(), tmp_hash.end());
        query_hash_hash.push_back(tmp_hash);
    }
}



// find candidate matches with LSH
void neighbor_query(const vector < vector <uint32_t> >& query_hash,
		      const uint64_t* lsh,
                      vector < set<uint32_t> >& candidates,
                      CMinHashFile& mhfile,
                      int num_hashes,
                      int min_hits,
                      double thresh,
		      TBlastKmerPrelimScoreVector& score_vector,
		      BlastKmerStats& kmer_stats,
		      int kmerVer)
{
	int num_chunks=query_hash.size();

	vector< vector<uint32_t> > candidate_oids;
	candidate_oids.resize(num_chunks);

	for (int num=0; num<num_chunks; num++)
	{
		for(set<uint32_t>::iterator i=candidates[num].begin(); i != candidates[num].end(); ++i)
		{
			uint64_t offset = lsh[(*i)];
			if(offset != 0)
			{
				int index=(*i)+1;
				while (lsh[index] == 0)
					index++;
				uint64_t read_size = (lsh[index] - offset)/4;
				int remaining = (int) read_size;
				int* oid_offset = mhfile.GetHits(offset);
				index=0;
				while (index < remaining)
				{
					int oid = *(oid_offset+index);
					candidate_oids[num].push_back(oid);
					index++;
				}
			}
		}
	}

	for (int index=0; index<num_chunks; index++)
	{
		kmer_stats.hit_count += candidates[index].size();
		kmer_stats.oids_considered += candidate_oids[index].size();
	}

	vector < vector <uint32_t> > query_hash_hash; 
	s_HashHashQuery(query_hash, query_hash_hash, mhfile.GetDataWidth(), mhfile.GetVersion());

	vector <uint32_t> subject_hash;
	subject_hash.resize(num_hashes);

	map< int, double > score_map;
	typedef map< int, double >::value_type score_mapValType;
	// for each candidate with hits estimate Jaccard distance.
	for (int n=0; n<num_chunks; n++)
	{
		std::sort(candidate_oids[n].begin(), candidate_oids[n].end());
		int last_oid = -1;
		int oid_count=1;
		for(vector<uint32_t>::iterator i = candidate_oids[n].begin(); i != candidate_oids[n].end(); ++i)
		{  	// check that min_hits criteria satisfied.
			if (last_oid == *i)
			{
				oid_count++;
				if (min_hits != oid_count)
					continue;
			}
			else
			{
				last_oid = *i;
				oid_count = 1;
				if (min_hits > 1)
					continue;
			}
				
			kmer_stats.jd_oid_count++;
	
			int oid = *i;
   			int subject_oid=0;
			mhfile.GetMinHits(oid, subject_oid, subject_hash);
			double current_score=0;
			if (kmerVer < 3)
				current_score = estimate_jaccard(query_hash_hash[n], subject_hash, num_hashes);
			else
				current_score = estimate_jaccard2(query_hash_hash[n], subject_hash, num_hashes);
			kmer_stats.jd_count++;
			if (current_score < thresh)
				continue;
  // cerr << "OID and score " << oid << " " << total_score << '\n';

			if (!score_map.count(subject_oid))
				score_map.insert(score_mapValType(subject_oid, current_score));
			else if (current_score > score_map[subject_oid])
					score_map[subject_oid] = current_score;
		}
	}

	// Print out the score
	for(map<int, double>::iterator i = score_map.begin(); i != score_map.end(); ++i)
	{
		double total_score = (*i).second;
		if (total_score > thresh)
		{
			score_vector.push_back((*i));
			kmer_stats.total_matches++;
		}
	}
}

static int
s_BlastKmerVerifyVolume(CMinHashFile& mhfile, string& error_msg, int volume)
{
	int num_hashes=mhfile.GetNumHashes();
	if(num_hashes < 32)
	{
		error_msg = "Only " + NStr::NumericToString(num_hashes) + " hashes in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	int kmerNum = mhfile.GetKmerSize();
	if (kmerNum < 4)
	{
		error_msg = "Kmer size is only " + NStr::NumericToString(kmerNum) + " in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	int numSeqs = mhfile.GetNumSeqs();
	if (numSeqs < 1)
	{
		error_msg = "Only " + NStr::NumericToString(numSeqs) + " sequences in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	int lshSize = mhfile.GetLSHSize();
	if (lshSize < 1000)
	{
		error_msg = "LSH size is only " + NStr::NumericToString(lshSize) + " in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	uint64_t* lsh_array = mhfile.GetLSHArray();
	int i=0;
	for (i=0; i<lshSize; i++)
	{
		if(lsh_array[i] > 0)
			break;
	}
	if (i == lshSize)
	{
		error_msg = "LSH array is empty in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	int subjectOid=0;
	vector<uint32_t> hits;
	mhfile.GetMinHits(3, subjectOid, hits);
	if (subjectOid < 0)
	{
		error_msg = "Subject OID is less than zero: " + NStr::NumericToString(subjectOid) + " in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	if (hits.size() != static_cast<size_t>(num_hashes))
	{
		error_msg = "Signature array only has only " + NStr::NumericToString((int) hits.size()) + " entries in volume " + NStr::NumericToString(volume); 
		return 1;
	}
	if (mhfile.GetVersion() == 3)
	{
		uint32_t last = 0;
        	for(vector<uint32_t>::iterator iter=hits.begin(); iter != hits.end(); ++iter)
		{
			if (last > *iter)
			{
				error_msg = "Signature array for version 3 index not in ascending order in volume " + NStr::NumericToString(volume);
				return 1;
			}
			last = *iter;
		}
	}
	return 0;
}

int
BlastKmerVerifyIndex(CRef<CSeqDB> seqdb, string &error_msg)
{
	int status=0;
	vector<string> kmerFiles;
	seqdb->FindVolumePaths(kmerFiles, false);
	int numFiles = kmerFiles.size();
	for (int i=0; i<numFiles; i++)
	{
		try {
			CMinHashFile mhfile(kmerFiles[i]);
			status = s_BlastKmerVerifyVolume(mhfile, error_msg, i);
		} 
		catch (const blast::CMinHashException& e)  {
			error_msg = e.GetMsg();
			status = 1;
 		}
		catch (const CFileException& e)  {
			error_msg = e.GetMsg();
			status = 1;
 		}
		catch (...)  {
			error_msg = "Unknown error";
			status = 1;
 		}
		if (status != 0)
			break;
	}
	return status;
}

END_NCBI_SCOPE
