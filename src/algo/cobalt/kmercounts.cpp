/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: kmercounts.cpp

Author: Greg Boratyn

Contents: Implementation of k-mer counting classes

******************************************************************************/


#include <ncbi_pch.hpp>

#include <math.h>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbiexpt.hpp>

#include <objmgr/seq_vector.hpp>

#include <algo/cobalt/kmercounts.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

// Default values for default params
unsigned int CSparseKmerCounts::sm_KmerLength = 4;
unsigned int CSparseKmerCounts::sm_AlphabetSize = kAlphabetSize;
vector<Uint1> CSparseKmerCounts::sm_TransTable;
bool CSparseKmerCounts::sm_UseCompressed = false;

static const Uint1 kXaa = 21;


CSparseKmerCounts::CSparseKmerCounts(const blast::SSeqLoc& seq)
{
    Reset(seq);
}

// Initializes position bit vector for k-mer counting
// modifies index
bool CSparseKmerCounts::InitPosBits(const objects::CSeqVector& sv, Uint4& pos,
                                     unsigned int& index,
                                     Uint4 num_bits, Uint4 kmer_len)

{
    pos = 0;
    unsigned i = 0;
    while (index + kmer_len - 1 < sv.size() && i < kmer_len) {

        // Skip kmers that contain X (unspecified aa)
        if (sv[index + i] == kXaa) {
            index += i + 1;
            
            pos = 0;
            i = 0;
            continue;
        }
        pos |= GetAALetter(sv[index + i]) << (num_bits * (kmer_len - i - 1));
        i++;
    }
    
    if (i < kmer_len) {
        return false;
    }

    index += i;
    return true;
}


static void MarkUsed(Uint4 pos, vector<Uint4>& entries, int chunk)
{
    int index = pos / chunk;
    int offset = pos - index * chunk;
    Uint4 mask = 0x80000000 >> offset;
    entries[index] |= mask;
}


void CSparseKmerCounts::Reset(const blast::SSeqLoc& seq)
{
    unsigned int kmer_len = sm_KmerLength;
    unsigned int alphabet_size = sm_AlphabetSize;

    _ASSERT(kmer_len > 0 && alphabet_size > 0);
    _ASSERT(sm_UseCompressed && !sm_TransTable.empty() 
           || !sm_UseCompressed && sm_TransTable.empty());

    if (!seq.seqloc->IsWhole() && !seq.seqloc->IsInt()) {
        NCBI_THROW(CKmerCountsException, eUnsupportedSeqLoc, 
                   "Unsupported SeqLoc encountered");
    }

    objects::CSeqVector sv(*seq.seqloc, *seq.scope);

    unsigned int num_elements;
    unsigned int seq_len = sv.size();
    TCount* counts = NULL;

    m_SeqLength = sv.size();

    m_Counts.clear();

    // Compute number of bits needed to represent all letters
    unsigned int mask = 1;
    int num = 0;
    while (alphabet_size > mask) {
        mask <<= 1;
        num++;
    }
    const int kNumBits = num;

    // Vecotr of counts is first computed using regular vector that is later
    // converted to the sparse vector (list of position-value pairs).
    // Positions are calculated as binary representations of k-mers, if they
    // fit in 32 bits.
    if (kmer_len * kNumBits < 32) {

        num_elements = 1 << (kNumBits * kmer_len);
        const Uint4 kMask = num_elements - (1 << kNumBits);
        counts = new TCount[num_elements];
        _ASSERT(counts);
        memset(counts, 0, num_elements * sizeof(TCount));

        const int kBitChunk = sizeof(Uint4) * 8;

        // Vector indicating non-zero elements
        vector<Uint4> used_entries(num_elements / kBitChunk + 1);

        //first k-mer
        Uint4 i = 0;
        Uint4 pos;
        bool is_pos = InitPosBits(sv, pos, i, kNumBits, kmer_len);
        counts[pos]++;
        MarkUsed(pos, used_entries, kBitChunk);

        //for each next kmer
        for (;i < seq_len && is_pos;i++) {

            // Kmers that contain unspecified amino acid X are not considered
            if (sv[i] == kXaa) {
                i++;
                is_pos = InitPosBits(sv, pos, i, kNumBits, kmer_len);

                if (i >= seq_len || !is_pos) {
                    break;
                }
            }

            pos <<= kNumBits;
            pos &= kMask;
            pos |= GetAALetter(sv[i]);
            counts[pos]++;
            MarkUsed(pos, used_entries, kBitChunk);
        }

        // Convert to sparse vector
        int ind = 0;
        while (ind < (int)(num_elements / kBitChunk + 1)) {
            while (ind < (int)(num_elements / kBitChunk + 1)
                   && used_entries[ind] == 0) {
                ind++;
            }

            for (Uint4 mask=0x80000000,j=0;used_entries[ind] != 0;
                 j++, mask>>=1) {
                _ASSERT(j < 32);
                if ((used_entries[ind] & mask) != 0) {
                    pos = ind * kBitChunk + j;

                    _ASSERT(counts[pos] > 0);
                    m_Counts.push_back(SVectorElement(pos, counts[pos]));

                    used_entries[ind] ^= mask;
                }
            }
            ind++;
        }

    }
    else {
        _ASSERT(pow((double)alphabet_size, (double)kmer_len) 
                < numeric_limits<Uint4>::max());


        AutoArray<double> base(kmer_len);
        for (Uint4 i=0;i < kmer_len;i++) {
            base[i] = pow((double)alphabet_size, (double)i);
        }
   
        num_elements = (Uint4)pow((double)alphabet_size, (double)kmer_len);
        counts = new TCount[num_elements];
        _ASSERT(counts);
        memset(counts, 0, num_elements * sizeof(TCount));

        for (unsigned i=0;i < seq_len - kmer_len + 1;i++) {

            // Kmers that contain unspecified amino acid X are not considered
            if (sv[i + kmer_len - 1] == kXaa) {
                i += kmer_len - 1;
                continue;
            }

            Uint4 pos = GetAALetter(sv[i]) - 1;
            _ASSERT(pos >= 0);
            _ASSERT(GetAALetter(sv[i]) <= alphabet_size);
            for (Uint4 j=1;j < kmer_len;j++) {
                pos += (Uint4)(((double)GetAALetter(sv[i + j]) - 1) * base[j]);
                _ASSERT(pos >= 0);
                _ASSERT(GetAALetter(sv[i + j]) <= alphabet_size);
            }
            counts[pos]++;
        }

        unsigned int ind = 0;
        for (Uint4 i=0;i < num_elements;i++) {
            if (counts[i] > 0) {
                m_Counts.push_back(SVectorElement(i, counts[i]));
            }
        }
        m_Counts.resize(ind);
    }

    if (counts) {
        delete [] counts;
    }
    
}

double CSparseKmerCounts::FractionCommonKmersDist(
                                                  const CSparseKmerCounts& v1,
                                                  const CSparseKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2, true);
    
    unsigned int len_seq1 = v1.GetSeqLength();
    unsigned int len_seq2 = v2.GetSeqLength();
    unsigned int smaller_len =  len_seq1 < len_seq2 ? len_seq1 : len_seq2;
    return 1.0 - (double) num_common/ (double)(smaller_len - GetKmerLength()
                                                + 1);    
}


double CSparseKmerCounts::FractionCommonKmersGlobalDist(
                                                  const CSparseKmerCounts& v1,
                                                  const CSparseKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2, true);
    
    unsigned int len_seq1 = v1.GetSeqLength();
    unsigned int len_seq2 = v2.GetSeqLength();
    unsigned int larger_len =  len_seq1 > len_seq2 ? len_seq1 : len_seq2;
    return 1.0 - (double) num_common/ (double)(larger_len - GetKmerLength()
                                                + 1);    
}


unsigned int CSparseKmerCounts::CountCommonKmers(
                                              const CSparseKmerCounts& vect1, 
                                              const CSparseKmerCounts& vect2,
                                              bool repetitions)

{

    unsigned int result = 0;
    TNonZeroCounts_CI it1 = vect1.m_Counts.begin();
    TNonZeroCounts_CI it2 = vect2.m_Counts.begin();

    // Iterating through non zero counts in both vectors
    do {
        // For each vector element that is non zero in vect1 and vect2
        while (it1 != vect1.m_Counts.end() && it2 != vect2.m_Counts.end() 
               && it1->position == it2->position) {

            // Increase number of common kmers found
            if (repetitions) {
                result += (unsigned)(it1->value < it2->value 
                                     ? it1->value : it2->value);
            }
            else {
                result++;
            }
            ++it1;
            ++it2;
        }

        //Finding the next pair of non-zero element in both vect1 and vect2

        while (it1 != vect1.m_Counts.end() && it2 != vect2.m_Counts.end() 
               && it1->position < it2->position) {
            ++it1;
        }

        while (it1 != vect1.m_Counts.end() && it2 != vect2.m_Counts.end() 
               && it2->position < it1->position) {
            ++it2;
        }


    } while (it1 != vect1.m_Counts.end() && it2 != vect2.m_Counts.end());

    return result;
}


CNcbiOstream& operator<<(CNcbiOstream& ostr, CSparseKmerCounts& cv)
{
    for (CSparseKmerCounts::TNonZeroCounts_CI it=cv.BeginNonZero();
         it != cv.EndNonZero();++it) {
        ostr << it->position << ":" << (int)it->value << " ";
    }
    ostr << NcbiEndl;

    return ostr;
}




