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
CSparseKmerCounts::TCount* CSparseKmerCounts::sm_Buffer = NULL;
bool CSparseKmerCounts::sm_ForceSmallerMem = false;

unsigned int CBinaryKmerCounts::sm_KmerLength = 3;
unsigned int CBinaryKmerCounts::sm_AlphabetSize = kAlphabetSize;
vector<Uint1> CBinaryKmerCounts::sm_TransTable;
bool CBinaryKmerCounts::sm_UseCompressed = false;

static const Uint1 kXaa = 21;


CSparseKmerCounts::CSparseKmerCounts(const objects::CSeq_loc& seq,
                                     objects::CScope& scope)
{
    Reset(seq, scope);
}

// Initialize position bit vector for k-mer counting
// @param sv Sequence [in]
// @param pos Index of the k-mer count [out]
// @param index Index of letter in sequence [in|out]
// @param num_bits Number of bits per letter in pos [in]
// @param kmer_len K-mer length [in]
// @return True if a valid k-mer was found, false otherwise
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


CSparseKmerCounts::TCount* CSparseKmerCounts::ReserveCountsMem(
                                                       unsigned int num_bits)
{
    Uint4 num_elements;
    TCount* counts = NULL;

    // Reserve memory for storing counts
    // there are two methods for indexing counts (see the Reset() method)
    // if memory cannot be allocated try to allocate for the second method
    // that requires less memory
    if (!sm_ForceSmallerMem && sm_KmerLength * num_bits 
                                  < kLengthBitsThreshold) {

        num_elements = 1 << (num_bits * sm_KmerLength);
        try {
            counts = new TCount[num_elements];
        }
        catch (std::bad_alloc) {
            sm_ForceSmallerMem = true;
            num_elements = (Uint4)pow((double)sm_AlphabetSize,
                                      (double)sm_KmerLength);

            try {
                counts = new TCount[num_elements];
            }
            catch (std::bad_alloc) {
                NCBI_THROW(CKmerCountsException, eMemoryAllocation,
                           "Memory cannot be allocated for k-mer counting."
                           " Try using compressed alphabet or smaller k.");
            }
                
        }
    }
    return counts;
}

void CSparseKmerCounts::Reset(const objects::CSeq_loc& seq,
                              objects::CScope& scope)
{
    unsigned int kmer_len = sm_KmerLength;
    unsigned int alphabet_size = sm_AlphabetSize;

    _ASSERT(kmer_len > 0 && alphabet_size > 0);

    if (sm_UseCompressed && sm_TransTable.empty()) {
        NCBI_THROW(CKmerCountsException, eInvalidOptions,
                   "Compressed alphabet selected, but translation table not"
                   " specified");
    }

    if (!seq.IsWhole() && !seq.IsInt()) {
        NCBI_THROW(CKmerCountsException, eUnsupportedSeqLoc, 
                   "Unsupported SeqLoc encountered");
    }

    _ASSERT(seq.GetId());
    objects::CSeqVector sv = scope.GetBioseqHandle(*seq.GetId()).GetSeqVector();

    unsigned int num_elements;
    unsigned int seq_len = sv.size();

    m_SeqLength = sv.size();
    m_Counts.clear();
    m_NumCounts = 0;

    if (m_SeqLength < kmer_len) {
        NCBI_THROW(CKmerCountsException, eBadSequence,
                   "Sequence shorter than desired k-mer length");
    }

    // Compute number of bits needed to represent all letters
    unsigned int mask = 1;
    int num = 0;
    while (alphabet_size > mask) {
        mask <<= 1;
        num++;
    }
    const int kNumBits = num;

    TCount* counts = sm_Buffer ? sm_Buffer : ReserveCountsMem(kNumBits); 

    // Vecotr of counts is first computed using regular vector that is later
    // converted to the sparse vector (list of position-value pairs).
    // Positions are calculated as binary representations of k-mers, if they
    // fit in 32 bits. Otherwise as numbers in system with base alphabet size.
    if (!sm_ForceSmallerMem && kmer_len * kNumBits < kLengthBitsThreshold) {

        num_elements = 1 << (kNumBits * kmer_len);
        const Uint4 kMask = num_elements - (1 << kNumBits);

        _ASSERT(counts);
        memset(counts, 0, num_elements * sizeof(TCount));

        const int kBitChunk = sizeof(Uint4) * 8;

        // Vector indicating non-zero elements
        vector<Uint4> used_entries(num_elements / kBitChunk + 1);

        //first k-mer
        Uint4 i = 0;
        Uint4 pos;
        bool is_pos = InitPosBits(sv, pos, i, kNumBits, kmer_len);
        if (is_pos) {
            _ASSERT(pos < num_elements);
            counts[pos]++;
            MarkUsed(pos, used_entries, kBitChunk);
            m_NumCounts++;

            //for each next kmer
            for (;i < seq_len && is_pos;i++) {

                if (GetAALetter(sv[i]) >= alphabet_size) {
                    NCBI_THROW(CKmerCountsException, eBadSequence,
                               "Letter out of alphabet in sequnece");
                }

                // Kmers that contain unspecified amino acid X are not 
                // considered
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
                _ASSERT(pos < num_elements);
                counts[pos]++;
                MarkUsed(pos, used_entries, kBitChunk);
                m_NumCounts++;
            }
        }

        // Convert to sparse vector
        m_Counts.reserve(m_SeqLength - kmer_len + 1);
        size_t ind = 0;
        Uint4 num_bit_chunks = num_elements / kBitChunk + 1;
        while (ind < num_elements / kBitChunk + 1) {

            // find next chunk with at least one non-zero count
            while (ind < num_bit_chunks && used_entries[ind] == 0) {
                ind++;
            }

            if (ind == num_bit_chunks) {
                break;
            }

            // find the set bit and get position in the counts vector
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

        _ASSERT(counts);
        memset(counts, 0, num_elements * sizeof(TCount));

        // Vector indicating non-zero elements
        const int kBitChunk = sizeof(Uint4) * 8;
        vector<Uint4> used_entries(num_elements / kBitChunk + 1);

        Uint4 pos;
        for (unsigned i=0;i < seq_len - kmer_len + 1;i++) {

            // Kmers that contain unspecified amino acid X are not considered
            if (sv[i + kmer_len - 1] == kXaa) {
                i += kmer_len - 1;
                continue;
            }

            pos = GetAALetter(sv[i]) - 1;
            _ASSERT(GetAALetter(sv[i]) <= alphabet_size);
            for (Uint4 j=1;j < kmer_len;j++) {
                pos += (Uint4)(((double)GetAALetter(sv[i + j]) - 1) * base[j]);
                _ASSERT(GetAALetter(sv[i + j]) <= alphabet_size);
            }
            counts[pos]++;
            MarkUsed(pos, used_entries, kBitChunk);
            m_NumCounts++;
        }

        // Convert to sparse vector
        m_Counts.reserve(m_SeqLength - kmer_len + 1);
        size_t ind = 0;
        Uint4 num_bit_chunks = num_elements / kBitChunk + 1;
        while (ind < num_elements / kBitChunk + 1) {

            // find next chunk with at least one non-zero count
            while (ind < num_bit_chunks && used_entries[ind] == 0) {
                ind++;
            }

            if (ind == num_bit_chunks) {
                break;
            }

            // find the set bit and get position in the counts vector
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

    if (!sm_Buffer && counts) {
        delete [] counts;
    }
    
}

double CSparseKmerCounts::FractionCommonKmersDist(
                                                  const CSparseKmerCounts& v1,
                                                  const CSparseKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2, true);
    
    unsigned int num_counts1 = v1.GetNumCounts();
    unsigned int num_counts2 = v2.GetNumCounts();
    unsigned int fewer_counts
        =  num_counts1 < num_counts2 ? num_counts1 : num_counts2;

    // In RC Edgar, BMC Bioinformatics 5:113, 2004 the denominator is
    // SeqLen - k + 1 that is equal to number of counts only if sequence
    // does not contain Xaa.
    return 1.0 - (double)num_common / (double)fewer_counts;
}


double CSparseKmerCounts::FractionCommonKmersGlobalDist(
                                                  const CSparseKmerCounts& v1,
                                                  const CSparseKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2, true);
    
    unsigned int num_counts1 = v1.GetNumCounts();
    unsigned int num_counts2 = v2.GetNumCounts();
    unsigned int more_counts
        = num_counts1 > num_counts2 ? num_counts1 : num_counts2;

    // In RC Edgar, BMC Bioinformatics 5:113, 2004 the denominator is
    // SeqLen - k + 1 that is equal to number of counts only if sequence
    // does not contain Xaa.
    return 1.0 - (double)num_common / (double)more_counts;
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


void CSparseKmerCounts::PreCount(void)
{
    // Reserve memory for storing counts of all possible k-mers
    // compute number of bits needed to represent all letters
    unsigned int mask = 1;
    int num_bits = 0;
    while (sm_AlphabetSize > mask) {
        mask <<= 1;
        num_bits++;
    }

    sm_Buffer = ReserveCountsMem(num_bits);
}

void CSparseKmerCounts::PostCount(void)
{
    if (sm_Buffer) {
        delete [] sm_Buffer;
    }
    sm_Buffer = NULL;
    sm_ForceSmallerMem = false;
}


CNcbiOstream& CSparseKmerCounts::Print(CNcbiOstream& ostr) const
{
    for (CSparseKmerCounts::TNonZeroCounts_CI it=BeginNonZero();
         it != EndNonZero();++it) {
        ostr << it->position << ":" << (int)it->value << " ";
    }
    ostr << NcbiEndl;

    return ostr;
}


CBinaryKmerCounts::CBinaryKmerCounts(const objects::CSeq_loc& seq,
                                     objects::CScope& scope)
{
    Reset(seq, scope);
}


void CBinaryKmerCounts::Reset(const objects::CSeq_loc& seq,
                              objects::CScope& scope)
{
    unsigned int kmer_len = sm_KmerLength;
    unsigned int alphabet_size = sm_AlphabetSize;

    _ASSERT(kmer_len > 0 && alphabet_size > 0);

    if (sm_UseCompressed && sm_TransTable.empty()) {
        NCBI_THROW(CKmerCountsException, eInvalidOptions,
                   "Compressed alphabet selected, but translation table not"
                   " specified");
    }

    if (!seq.IsWhole() && !seq.IsInt()) {
        NCBI_THROW(CKmerCountsException, eUnsupportedSeqLoc, 
                   "Unsupported SeqLoc encountered");
    }

    _ASSERT(seq.GetId());
    objects::CSeqVector sv = scope.GetBioseqHandle(*seq.GetId()).GetSeqVector();

    unsigned int num_elements;
    unsigned int seq_len = sv.size();

    m_SeqLength = sv.size();
    m_Counts.clear();
    m_NumCounts = 0;

    if (m_SeqLength < kmer_len) {
        NCBI_THROW(CKmerCountsException, eBadSequence,
                   "Sequence shorter than desired k-mer length");
    }

    const int kBitChunk = sizeof(Uint4) * 8;

    // Vecotr of counts is first computed using regular vector that is later
    // converted to the sparse vector (list of position-value pairs).
    // Positions are calculated as binary representations of k-mers, if they
    // fit in 32 bits. Otherwise as numbers in system with base alphabet size.

    _ASSERT(pow((double)alphabet_size, (double)kmer_len) 
            < numeric_limits<Uint4>::max());

    AutoArray<double> base(kmer_len);
    for (Uint4 i=0;i < kmer_len;i++) {
        base[i] = pow((double)alphabet_size, (double)i);
    }
   
    num_elements = (Uint4)pow((double)alphabet_size, (double)kmer_len);

    m_Counts.resize(num_elements / 32 + 1, (Uint4)0);

    Uint4 pos;
    unsigned i = 0;

    // find the first k-mer that does not contain Xaa
    bool is_xaa;
    do {
        is_xaa = false;
        for (unsigned j=0;j < kmer_len && i < seq_len - kmer_len + 1;j++) {

            if (sv[i + j] == kXaa) {
                i += kmer_len;
                is_xaa = true;
                break;
            }
        }
    } while (i < seq_len - kmer_len + 1 && is_xaa);
    // if sequences contains only Xaa's then exit
    if (i >= seq_len - kmer_len + 1) {
        return;
    }

    // for each subsequence of kmer_len residues
    for (;i < seq_len - kmer_len + 1;i++) {

        // k-mers that contain unspecified amino acid X are not considered
        if (sv[i + kmer_len - 1] == kXaa) {

            // move k-mer window past Xaa
            i += kmer_len;

            // find first k-mer that does not contain Xaa
            do {
                is_xaa = false;
                for (unsigned j=0;j < kmer_len && i < seq_len - kmer_len + 1;
                     j++) {

                    if (sv[i + j] == kXaa) {
                        i += kmer_len;
                        is_xaa = true;
                        break;
                    }
                }
            } while (i < seq_len - kmer_len + 1 && is_xaa);

            // if Xaa are found till the end of sequence exit
            if (i >= seq_len - kmer_len + 1) {
                return;
            }
        }

        pos = GetAALetter(sv[i]) - 1;
        _ASSERT(GetAALetter(sv[i]) <= alphabet_size);
        for (Uint4 j=1;j < kmer_len;j++) {
            pos += (Uint4)(((double)GetAALetter(sv[i + j]) - 1) * base[j]);
            _ASSERT(GetAALetter(sv[i + j]) <= alphabet_size);
        }
        MarkUsed(pos, m_Counts, kBitChunk);
    }

    m_NumCounts = 0;
    for (size_t i=0;i < m_Counts.size();i++) {
        m_NumCounts += x_Popcount(m_Counts[i]);
    }
}


double CBinaryKmerCounts::FractionCommonKmersDist(const CBinaryKmerCounts& v1,
                                                  const CBinaryKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2);
    
    unsigned int num_counts1 = v1.GetNumCounts();
    unsigned int num_counts2 = v2.GetNumCounts();
    unsigned int fewer_counts
        =  num_counts1 < num_counts2 ? num_counts1 : num_counts2;

    // In RC Edgar, BMC Bioinformatics 5:113, 2004 the denominator is
    // SeqLen - k + 1 that is equal to number of counts only if sequence
    // does not contain Xaa.
    return 1.0 - (double)num_common / (double)fewer_counts;
}


double CBinaryKmerCounts::FractionCommonKmersGlobalDist(
                                                  const CBinaryKmerCounts& v1,
                                                  const CBinaryKmerCounts& v2)
{
    _ASSERT(GetKmerLength() > 0);

    unsigned int num_common = CountCommonKmers(v1, v2);
    
    unsigned int num_counts1 = v1.GetNumCounts();
    unsigned int num_counts2 = v2.GetNumCounts();
    unsigned int more_counts
        = num_counts1 > num_counts2 ? num_counts1 : num_counts2;

    // In RC Edgar, BMC Bioinformatics 5:113, 2004 the denominator is
    // SeqLen - k + 1 that is equal to number of counts only if sequence
    // does not contain Xaa.
    return 1.0 - (double)num_common / (double)more_counts;
}


unsigned int CBinaryKmerCounts::CountCommonKmers(
                                              const CBinaryKmerCounts& vect1, 
                                              const CBinaryKmerCounts& vect2)
{
    unsigned int result = 0;
    const Uint4* counts1 = &vect1.m_Counts[0];
    const Uint4* counts2 = &vect2.m_Counts[0];
    int size = vect1.m_Counts.size();

    for (int i=0;i < size;i++) {
        result += x_Popcount(counts1[i] & counts2[i]);
    }

    return result;
}
