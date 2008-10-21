#ifndef ALGO_COBALT___KMERCOUNTS__HPP
#define ALGO_COBALT___KMERCOUNTS__HPP

/* $Id$
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

File name: kmercounts.hpp

Author: Greg Boratyn

Contents: Interface for k-mer counting

******************************************************************************/


#include <util/math/matrix.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/scope.hpp>
#include <algo/cobalt/base.hpp>
#include <algo/blast/core/blast_encoding.h>
#include <vector>
#include <stack>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


// TODO: Implement binary k-mer counts vector

// Compressed alphabets taken from 
// Shiryev et al.(2007),  Bioinformatics, 23:2949-2951
// 23-to-10 letter compressed alphabet. Based on SE-V(10)
static const string kAlphabet10("IJLMV AST BDENZ KQR G FY P H C W");
// 23-to-15 letter compressed alphabet. Based on SE_B(14) 
static const string kAlphabet15("ST IJV LM KR EQZ A G BD P N F Y H C W");


/// Kmer counts for alignment free sequence similarity computation
/// implemented as a sparse vector
///
class CSparseKmerCounts
{
public:
    typedef Uint1 TCount;


    /// Element of the sparse vector
    typedef struct SVectorElement {
    Uint4 position;   ///< position of non-zero element
    TCount value;     ///< value of non-zero element

    /// Default constructor
    SVectorElement(void) {position = 0; value = 0;}

    /// Create vector element
    /// @param pos Element position
    /// @param val Element value
    SVectorElement(Uint4 pos, TCount val) {position = pos; value = val;}
    } SVectorElement;

    typedef vector<SVectorElement>::const_iterator TNonZeroCounts_CI;
    
    
public:
    /// Create empty counts vector
    ///
    CSparseKmerCounts(void) : m_SeqLength(0) {}

    /// Create k-mer counts vector from SSeqLoc with defalut k-mer length and
    /// alphabet size
    /// @param seq The sequence to be represented as k-mer counts [in]
    /// @param scope Scope
    ///
    CSparseKmerCounts(const objects::CSeq_loc& seq,
                      objects::CScope& scope);

    /// Reset the counts vector
    /// @param seq Sequence
    ///
    void Reset(const objects::CSeq_loc& seq, objects::CScope& scope);

    /// Get sequence length
    /// @return Sequence length
    ///
    unsigned int GetSeqLength(void) const {return m_SeqLength;}

    /// Get default kmer length
    /// @return Default k-mer length
    ///
    static unsigned int GetKmerLength(void) 
    {return CSparseKmerCounts::sm_KmerLength;}

    /// Get default alphabet size
    /// @return Default alphabet size
    ///
    static unsigned int GetAlphabetSize(void) {return sm_AlphabetSize;}

    /// Get non-zero counts iterator
    /// @return Non-zero counts iterator pointing to the begining
    ///
    TNonZeroCounts_CI BeginNonZero(void) const {return m_Counts.begin();}

    /// Get non-zero counts iterator
    /// @return Non-zero counts iterator pointing to the end
    ///
    TNonZeroCounts_CI EndNonZero(void) const {return m_Counts.end();}

    /// Set default k-mer length
    /// @param len Default k-mer length [in]
    ///
    static void SetKmerLength(unsigned len) {sm_KmerLength = len;}

    /// Set Default alphabet size
    /// @param size Default alphabet size [in]
    ///
    static void SetAlphabetSize(unsigned size) {sm_AlphabetSize = size;}

    /// Set default compressed alphabet letter translation table
    /// @return Reference to translation table [in|out]
    ///
    static vector<Uint1>& SetTransTable(void) {return sm_TransTable;}

    /// Set default option for using compressed alphabet
    /// @param use_comp Will compressed alphabet be used [in]
    ///
    static void SetUseCompressed(bool use_comp) {sm_UseCompressed = use_comp;}

    /// Compute 1 - local fraction of common k-mers between two count vectors
    /// normalized by length of shorter sequence
    /// @param vect1 K-mer counts vector [in]
    /// @param vect2 K-mer counts vector [in]
    /// @return Local fraction of common k-mer as distance
    ///
    /// Computes 1 - F(v1, v2), where 
    /// F(x, y) = \sum_{t} \min \{n_x(t), n_y(t)\} / (\min \{L_x, L_y\} 
    /// - k + 1), where
    /// t - k-mer, n_x(t) - number of k-mer t in x, L_x - length of x, 
    /// k - k-mer length
    /// F(x, y) is described in RC Edgar, BMC Bioinformatics 5:113, 2004
    static double FractionCommonKmersDist(const CSparseKmerCounts& vect1, 
                      const CSparseKmerCounts& vect2);

    /// Compute 1 - global fraction of common k-mers between two count vectors
    /// normalized by length of longer sequence
    /// @param vect1 K-mer counts vector [in]
    /// @param vect2 K-mer counts vector [in]
    /// @return Global fraction of common k-mers as distance
    ///
    /// Computes 1 - F(v1, v2), where 
    /// F(x, y) = \sum_{t} \min \{n_x(t), n_y(t)\} / (\max \{L_x, L_y\} 
    /// - k + 1), where
    /// t - k-mer, n_x(t) - number of k-mer t in x, L_x - length of x, 
    /// k - k-mer length
    /// F(x, y) is modified version of measure presented 
    /// RC Edgar, BMC Bioinformatics 5:113, 2004
    static double FractionCommonKmersGlobalDist(const CSparseKmerCounts& v1,
                        const CSparseKmerCounts& v2);

    /// Copmute number of common kmers between two count vectors
    /// @param v1 K-mer counts vector [in]
    /// @param v2 K-mer counts vecotr [in]
    /// @param repetitions Should multiple copies of the same k-mer be counted
    /// @return Number of k-mers that are present in both counts vectors
    ///
    static unsigned int CountCommonKmers(const CSparseKmerCounts& v1, 
                     const CSparseKmerCounts& v2, 
                     bool repetitions = true);
private:
    static Uint4 GetAALetter(Uint1 letter)
    {return (Uint4)(sm_UseCompressed ? sm_TransTable[(int)letter] : letter);}

    /// Initializes element index as bit vector for first k letters, 
    /// skipping Xaa
    /// @param sv Sequence [in]
    /// @param pos Element index in sparse vector [out]
    /// @param index Index of letter in the sequence where k-mer counting
    /// starts. At exit index points to the next letter after first 
    /// k-mer [in|out]
    /// @param num_bits Number of bits in pos per letter [in]
    /// @param kmer_len K-mer length [in]
    /// @return True if pos was initialized, false otherwise (if no k-mer
    /// without X was found)
    static bool InitPosBits(const objects::CSeqVector& sv, Uint4& pos,
                            unsigned int& index,  Uint4 num_bits,
                            Uint4 kmer_len);


protected:
    vector<SVectorElement> m_Counts;
    unsigned int m_SeqLength;
    static unsigned int sm_KmerLength;
    static unsigned int sm_AlphabetSize;
    static vector<Uint1> sm_TransTable;
    static bool sm_UseCompressed;
};


CNcbiOstream& operator<<(CNcbiOstream& ostr, CSparseKmerCounts& cv);


/// Exception class for Kmer counts
class CKmerCountsException : public CException
{
public:
    enum EErrCode {
        eUnsupportedSeqLoc = 1, eUnsuportedDistMethod
    };

    virtual const char* GetErrCodeString(void) const {
        return "eUnsupportedSeqLoc";
    }

    NCBI_EXCEPTION_DEFAULT(CKmerCountsException, CException);
};


/// Creates translation table for compressed alphabets
/// @param trans_string String with groupped letters [in]
/// @param trans_table Translation table [out]
/// @param alphabet_len Number of letters in compressed alphabet
///
static void BuildCompressedTranslation(const string& trans_string,
                       vector<Uint1>& trans_table,
                       unsigned alphabet_len)
{
    Uint4 compressed_letter = 1; // this allows for gaps
    trans_table.clear();
    trans_table.resize(alphabet_len + 1, 0);
    for (Uint4 i = 0; i < trans_string.length();i++) {
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


/// Interface for computing and manipulating k-mer counts vectors that allows
/// for different implementations of K-mer counts vectors
///
template <class TKmerCounts>
class TKmerMethods
{
public:
    enum ECompressedAlphabet {
        eRegular, eSE_V10, eSE_B15
    };

    enum EDistMeasures {
        eFractionCommonKmersGlobal, 
        eFractionCommonKmersLocal
    };

    typedef CNcbiMatrix<double> TDistMatrix;

public:

    /// Set default counts vector parameters
    /// @param kmer_len K-mer length [in]
    /// @param alphabet_size Alphabet size [in]
    ///
    static void SetParams(unsigned kmer_len, unsigned alphabet_size)
    {
        TKmerCounts::SetKmerLength(kmer_len);
        TKmerCounts::SetAlphabetSize(alphabet_size);
        TKmerCounts::SetTransTable().clear();
        TKmerCounts::SetUseCompressed(false);
    }

    /// Set default counts vector parameters for use with compressed alphabet
    /// @param kmer_len K-mer length [in]
    /// @param alph Compressed alphabet to use [in]
    ///
    static void SetParams(unsigned kmer_len, ECompressedAlphabet alph) {
        TKmerCounts::SetKmerLength(kmer_len);
        unsigned int len;
        unsigned int compressed_len;
        switch (alph) {
        case eSE_V10:
            len = 28;
            compressed_len = 11; //including gap
            BuildCompressedTranslation(kAlphabet10, 
                                       TKmerCounts::SetTransTable(), 
                                       len);
            TKmerCounts::SetAlphabetSize(compressed_len);
            TKmerCounts::SetUseCompressed(true);
            break;
            
        case eSE_B15:
            len = 28;
            compressed_len = 16; //including gap
            BuildCompressedTranslation(kAlphabet15,
                                       TKmerCounts::SetTransTable(),
                                       len);
            TKmerCounts::SetAlphabetSize(compressed_len);
            TKmerCounts::SetUseCompressed(true);
            break;

        case eRegular:
            TKmerCounts::SetAlphabetSize(kAlphabetSize);
            TKmerCounts::SetTransTable().clear();
            TKmerCounts::SetUseCompressed(false);
        }
    }

    /// Create k-mer counts vectors for given sequences
    /// @param seqs List of sequences [in]
    /// @param counts List of k-mer counts vectors [out]
    ///
    static void ComputeCounts(const vector< CRef<objects::CSeq_loc> >& seqs,
                              objects::CScope& scope,
                              vector<TKmerCounts>& counts)
    {
        counts.clear();
    
        ITERATE(vector< CRef<objects::CSeq_loc> >, it, seqs) {
            counts.push_back(TKmerCounts(**it, scope));
        }
    }

    /// Compute matrix of distances between given counts vectors
    /// @param counts List of k-mer counts vectors [in]
    /// @param fsim Function that computes distance betwee two vectors [in]
    /// @param dmat Distance matrix [out]
    ///
    static void ComputeDistMatrix(const vector<TKmerCounts>& counts,
                  double(*fsim)(const TKmerCounts&, const TKmerCounts&),
                  TDistMatrix& dmat)
        
    {
        dmat.Resize(counts.size(), counts.size(), 0.0);
        for (size_t i=0;i < counts.size() - 1;i++) {
            for (size_t j=i+1;j < counts.size();j++) {
                dmat(i, j) = fsim(counts[i], counts[j]);
                dmat(j, i) = dmat(i, j);
            }
        }
    }

    /// Compute matrix of distances between given list of counts vectors
    /// using distance function with additional normalizing values
    /// @param counts List of k-mer counts vectors [in]
    /// @param dmat Distance matrix [out]
    /// @param fsim Function that computes distance betwee two vectors [in]
    /// @param normalizers List of normalizing arguments [in]
    ///
    static void ComputeDistMatrix(const vector<TKmerCounts>& counts,
        TDistMatrix& dmat,
        double(*fsim)(const TKmerCounts&, const TKmerCounts&, double, double),
        const vector<double>& normalizers);


    /// Compute distance matrix for given counts vectors and distance measure
    /// @param counts List of k-mer counts vecotrs [in]
    /// @param dist_method Distance measure [in]
    /// @param dmat Distance matrix [out]
    ///
    static void ComputeDistMatrix(const vector<TKmerCounts>& counts,
                                  EDistMeasures dist_method,
                                  TDistMatrix& dmat)
    {
        switch (dist_method) {
        case eFractionCommonKmersLocal:
            ComputeDistMatrix(counts, TKmerCounts::FractionCommonKmersDist, 
                              dmat);
            break;
        
        case eFractionCommonKmersGlobal:
            ComputeDistMatrix(counts, 
                              TKmerCounts::FractionCommonKmersGlobalDist, 
                              dmat);
            break;
        
        default:
            NCBI_THROW(CKmerCountsException, eUnsuportedDistMethod,
                       "Unrecognised distance measure");
        }
    }


    /// Compute distance matrix for given counts vectors and distance measure
    /// and avoid copying
    /// @param counts List of k-mer counts vecotrs [in]
    /// @param dist_method Distance measure [in]
    /// @return Distance matrix
    ///
    static auto_ptr<TDistMatrix> ComputeDistMatrix(
                                          const vector<TKmerCounts>& counts,
                                          EDistMeasures dist_method)
    {
        auto_ptr<TDistMatrix> dmat(new TDistMatrix(counts.size(), 
                                                   counts.size(), 0));
        ComputeDistMatrix(counts, dist_method, *dmat.get());
        return dmat;
    }
};



END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___KMERCOUNTS__HPP */
