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
 * Author:  Christiam Camacho
 *
 */

/** @file pssmcreate-cppunit.cpp
 * Unit test module for creation of PSSMs from multiple sequence alignments.
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbi_limits.hpp>

// Serial library includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <util/random_gen.hpp>
#include <util/math/matrix.hpp>

// Object includes
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

// ASN.1 definition for PSSM (scoremat)
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>

// BLAST includes
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/core/blast_setup.h>
#include <blast_objmgr_priv.hpp>
#include <blast_psi_priv.h>
#include <blast_posit.h>
#include "psiblast_aux_priv.hpp"    // for CScorematPssmConverter

#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/pssm_engine.hpp>

// Unit test auxiliary includes
#include "blast_test_util.hpp"
// #include "psiblast_test_util.hpp"

// Object manager includes
#include <objmgr/util/sequence.hpp>

// Standard scoring matrices
#include <util/tables/raw_scoremat.h>

// Seqport utilities
#include <objects/seq/seqport_util.hpp>

#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

/// This class exists merely to call private methods in CPsiBlastInputData
/// and CPssmEngine.  Both clases declare this one as a friend.
class CPssmCreateTestFixture {
public:
   /// Gets error strings from a CPssmEngine private method
   /// @param error_code input integer code
   static string 
   x_ErrorCodeToString(int error_code)
   {
         return CPssmEngine::x_ErrorCodeToString(error_code); 
   }

   /// Gets Subject sequence from a CPsiBlastInputData private method
   /// @param ds alignment input
   /// @param scope allos fetching of sequence
   /// @param sequence_data return value for sequence.
   static void
   x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope,
                          string& sequence_data)
   {
         return CPsiBlastInputData::x_GetSubjectSequence(ds, scope, sequence_data);
   }

   /// Accesses CPssmEngine private method.
   /// Copies query sequence and adds protein sentinel bytes at the beginning
   /// and at the end of the sequence.
   /// @param query sequence to copy [in]
   /// @param query_length length of the sequence above [in]
   /// @throws CBlastException if does not have enough memory
   /// @return copy of query guarded by protein sentinel bytes
   static unsigned char*
   x_GuardProteinQuery(const unsigned char* query,
                                 unsigned int query_length)
   {
         return CPssmEngine::x_GuardProteinQuery(query, query_length);
   }

   /// Accesses CPsiBlastInputData private method.  
   /// Returns the number of sequences that make up the multiple sequence
   /// alignment
   /// @param input Instance of CPsiBlastInputData
   static unsigned int 
   GetNumAlignedSequences(const CPsiBlastInputData& input)
   {
         return input.GetNumAlignedSequences();
   }
};


/******************************* copied from blast_psi_cxx.cpp **************/

/// Mock object for the PSSM input data which returns multiple sequence
/// alignment data which has flanking gaps
class CPssmInputFlankingGaps : public IPssmInputData
{
public:
    CPssmInputFlankingGaps() {
        const unsigned int  kQuerySize = 10;
        const unsigned int  kNumSeqs = 2;
        const unsigned char kQuery[] = { 3, 9, 14, 20, 6, 23, 1, 7, 16, 5 };

        m_query = new unsigned char[kQuerySize];
        memcpy((void*) m_query, (void*) kQuery, kQuerySize*sizeof(*kQuery));

        m_dim.query_length = kQuerySize;
        m_dim.num_seqs = kNumSeqs;

        m_msa = PSIMsaNew(&m_dim);

        for (unsigned int i = 0; i < m_dim.query_length; i++) {
            for (unsigned int j = 0; j < m_dim.num_seqs+1; j++) {
                m_msa->data[j][i].letter = kQuery[i];
                m_msa->data[j][i].is_aligned = true;
            }
        }

        // Add the flanking gaps
        m_msa->data[1][0].letter = 
            m_msa->data[2][0].letter = 
            m_msa->data[2][m_dim.query_length-1].letter = 
            AMINOACID_TO_NCBISTDAA[(int)'-'];

        m_options = NULL;
        PSIBlastOptionsNew(&m_options);

        // don't request any diagnostics data
        memset((void*) &m_diag_request, 0, sizeof(m_diag_request));
    }

    virtual ~CPssmInputFlankingGaps() {
        delete [] m_query;
        m_msa = PSIMsaFree(m_msa);
        m_options = PSIBlastOptionsFree(m_options);
    }

    void Process() {}
    unsigned char* GetQuery() { return m_query; }
    unsigned int GetQueryLength() { return m_dim.query_length; }
    PSIMsa* GetData() { return m_msa; }
    const PSIBlastOptions* GetOptions() { return m_options; }
    const PSIDiagnosticsRequest* GetDiagnosticsRequest() { 
        return &m_diag_request; 
    }

protected:

    unsigned char*          m_query;
    PSIMsaDimensions        m_dim;
    PSIMsa*                 m_msa;
    PSIBlastOptions*        m_options;
    PSIDiagnosticsRequest   m_diag_request;
};

/// Mock object for the PSSM input data which returns a query sequence with a
/// gap in it
class CPssmInputGapsInQuery : public CPssmInputFlankingGaps
{
public:
    CPssmInputGapsInQuery() {
        // initialize multiple sequence alignment data with valid data
        for (unsigned int i = 0; i < m_dim.query_length; i++) {
            for (unsigned int j = 0; j < m_dim.num_seqs+1; j++) {
                m_msa->data[j][i].letter = m_query[i];
                m_msa->data[j][i].is_aligned = true;
            }
        }

        // Randomly assign a position in the query to contain a gap
        CRandom r(time(NULL));
        int gap_position = r.GetRand(0, GetQueryLength() - 1);
        m_query[gap_position] = AMINOACID_TO_NCBISTDAA[(int)'-'];
        m_msa->data[0][gap_position].letter = m_query[gap_position];
    }
};

/// Mock object for the PSSM input data which returns a query sequence with a
/// gap in it
class CPssmInputQueryLength0 : public CPssmInputFlankingGaps
{
public:
    unsigned int GetQueryLength() { return 0; }
};

/// Mock object for the PSSM input data which returns NULLs for all its methods
class CNullPssmInput: public IPssmInputData
{
public:
    void Process() {}
    unsigned char* GetQuery() { return NULL; }
    unsigned int GetQueryLength() { return 0; }
    PSIMsa* GetData() { return NULL; }
    const PSIBlastOptions* GetOptions() { return NULL; }
    const char* GetMatrixName() { return NULL; }
    const PSIDiagnosticsRequest* GetDiagnosticsRequest() { return NULL; }
};

class CPssmInputUnsupportedMatrix : public CPssmInputFlankingGaps
{
public:
    const char* GetMatrixName() { return "TEST"; }
};

/// Mock object for the PSSM input data which can be configured to have
/// different combinations of aligned sequences. Currently used to test the
/// purging of biased sequences in multiple sequence alignment data
class CPssmInputTestData : public CPssmInputFlankingGaps
{
public:
    // Convenience for defining an aligned segment/region in the multiple
    // sequence alignment
    typedef pair<TSeqPos, TSeqPos> TAlignedSegment;

    // Enumeration to specify the various data setups that can be created with
    // this class
    enum EAlignmentType {
        eSelfHit,           // Single pairwise alignment which is a self hit
        eDuplicateHit,      // 2 pairwise alignments where hits 1 and 2 are 
                            // identical
        eNearIdenticalHits, // 2 pairswise alignments where hits 1 and 2 are
                            // 94% identical
        eMsaHasUnalignedRegion, // multiple sequence alignment with 3 sequences
                            // (including the query) which contain a region
                            // where the query is unaligned to any other
                            // sequences, i.e.:
                            //   query: AAAAAAAAAABBBBBBBCCCCCCCCCCC
                            //   sbjct: DDDDDDDDDD------------------
                            //   sbjct: -----------------EEEEEEEEEEE
        eQueryAlignedWithInternalGaps, // multiple sequence alignment with 2
                            // sequences which contain regions where internal
                            // (as opposed to flanking) gaps are aligned to the
                            // query sequence, i.e.:
// num_seqs: 1, query_length: 87                                                   
// MFKVYGYDSNIHKCGPCDNAKRLLTVKKQPFEFINIMPEKGVFDDEKIAELLTKLGRDTQIGLTMPQVFAPDGSHIGGFDQLREYFK
// KVVVFIKP----TCPFCRKTQELLSQLPFLLEFVDITAT--SDTNEIQDYLQQLTGA-----RTVPRVFIG-KECIGGCTDLESMHK
        eHenikoffsPaper
    };

    CPssmInputTestData(EAlignmentType type, PSIBlastOptions* opts = NULL) {

        // Clean up data allocated by parent class
        if (m_query) {
            delete [] m_query; 
            m_query = NULL;
            m_msa = PSIMsaFree(m_msa);
            m_options = PSIBlastOptionsFree(m_options);
        }

        PSIBlastOptionsNew(&m_options);
        if (opts) {
            memcpy((void*)&m_options, (void*)opts, sizeof(PSIBlastOptions));
        }

        switch (type) {
        case eSelfHit:
            SetupSelfHit();
            break;

        case eDuplicateHit:
            SetupDuplicateHit();
            break;

        case eNearIdenticalHits:
            SetupNearIdenticalHits();
            break;

        case eMsaHasUnalignedRegion:
            SetupMsaHasUnalignedRegion();
            break;

        case eQueryAlignedWithInternalGaps:
            SetupQueryAlignedWithInternalGaps();
            break;

        case eHenikoffsPaper:
            SetupHenikoffsPositionBasedSequenceWeights();
            break;

        default:
            throw std::logic_error("Unsupported alignment test data");
        }
    }

    ~CPssmInputTestData() {
        delete [] m_query;
        m_query = NULL;
        m_msa = PSIMsaFree(m_msa);
        m_options = PSIBlastOptionsFree(m_options);
    }


private:
// Gi 129295
static const size_t kQueryLength = 232;
static const Uint1 kQuery[kQueryLength];

     void SetupSelfHit(void) {
        const Uint4 kNumAlignedSeqs = 1; // does not include query

        m_dim.query_length = kQueryLength;
        m_dim.num_seqs = kNumAlignedSeqs;
        m_msa = PSIMsaNew(&m_dim);
        m_query = new unsigned char[kQueryLength];

        // Initialize sequence 1 with the query (self-hit)
        for (unsigned int i = 0; i < kQueryLength; i++) {
            for (unsigned int seq_idx = 0; seq_idx < kNumAlignedSeqs + 1;
                 seq_idx++) {
                m_msa->data[seq_idx][i].letter = m_query[i] = kQuery[i];
                m_msa->data[seq_idx][i].is_aligned = true;
            }
        }
    }

    Uint1 FindNonIdenticalHighScoringResidue
        (Uint1 res, const SNCBIPackedScoreMatrix* score_matrix)
    {
        BOOST_REQUIRE(score_matrix);
        Uint1 retval = AMINOACID_TO_NCBISTDAA[(int)'-'];
        int max_score = BLAST_SCORE_MIN;

        for (size_t i = 0; i < BLASTAA_SIZE; i++) {
            // alignment with itself is not allowed :)
            if (i == res) {
                continue;
            }
            int score = 
                static_cast<int>(NCBISM_GetScore(score_matrix, res, i));
            if (score > max_score) {
                max_score = score;
                retval = i;
            }
        }
        BOOST_REQUIRE(retval != AMINOACID_TO_NCBISTDAA[(int)'-']);
        return retval;
    }

    void SetupMsaHasUnalignedRegion(void) {
        const Uint4 kNumAlignedSeqs = 2;    // does not include query

        m_dim.query_length = kQueryLength;
        m_dim.num_seqs = kNumAlignedSeqs;
        m_msa = PSIMsaNew(&m_dim);
        m_query = new unsigned char[kQueryLength];

        // Initialize query sequence
        for (unsigned int i = 0; i < kQueryLength; i++) {
            m_msa->data[0][i].letter = m_query[i] = kQuery[i];
            m_msa->data[0][i].is_aligned = true;
        }

        const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;

        // Initialize sequence 1 with the highest scoring residues that can be
        // aligned with the query for the first 100 residues
        // This is done so that the aligned sequences are not purged in the
        // first stage of PSSM creation
        const TAlignedSegment kFirstAlignment(0, 100);
        for (unsigned int i = kFirstAlignment.first; 
             i < kFirstAlignment.second; i++) {
            m_msa->data[1][i].letter = 
                FindNonIdenticalHighScoringResidue(kQuery[i], score_matrix);
            m_msa->data[1][i].is_aligned = true;
        }

        // Initialize sequence 2 with the highest scoring residues that can be
        // aligned with the query for residue positions 200-kQueryLength
        // This is done so that the aligned sequences are not purged in the
        // first stage of PSSM creation
        const TAlignedSegment kSecondAlignment(200, kQueryLength);
        for (unsigned int i = kSecondAlignment.first; 
             i < kSecondAlignment.second; i++) {
            m_msa->data[2][i].letter = 
                FindNonIdenticalHighScoringResidue(kQuery[i], score_matrix);
            m_msa->data[2][i].is_aligned = true;
        }
    }

    void SetupQueryAlignedWithInternalGaps() {
        using std::pair;
        using std::string;
        using std::vector;

        const Uint4 kNumAlignedSeqs = 1;
        const size_t kLocalQueryLength = 87;

        m_dim.query_length = kLocalQueryLength;
        m_dim.num_seqs = kNumAlignedSeqs;
        m_msa = PSIMsaNew(&m_dim);
        m_query = new unsigned char[kLocalQueryLength];

        string query_seq("MFKVYGYDSNIHKCGPCDNAKRLLTVKKQPFEFINIM");
        query_seq += string("PEKGVFDDEKIAELLTKLGRDTQIGLTMPQVFAPDGSHIGGFD");
        query_seq += string("QLREYFK");

        typedef pair<TAlignedSegment, string> TAlignedSequence;
        vector<TAlignedSequence> aligned_sequence;

        TAlignedSequence region(make_pair(make_pair(0U, 8U), 
                                          string("KVVVFIKP")));
        aligned_sequence.push_back(region);

        region = make_pair(make_pair(12U, 39U),
                           string("TCPFCRKTQELLSQLPFLLEFVDITAT"));
        aligned_sequence.push_back(region);

        region = make_pair(make_pair(41U, 57U), string("SDTNEIQDYLQQLTGA"));
        aligned_sequence.push_back(region);

        region = make_pair(make_pair(62U, 71U), string("RTVPRVFIG"));
        aligned_sequence.push_back(region);

        region = make_pair(make_pair(72U, 87U), string("KECIGGCTDLESMHK"));
        aligned_sequence.push_back(region);


        const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
        for (Uint4 i = 0; i < kLocalQueryLength; i++) {
            m_query[i] = CSeqportUtil::GetIndex(CSeq_data::e_Ncbistdaa,
                                                query_seq.substr(i, 1));
            m_msa->data[0][i].letter = m_query[i];
            m_msa->data[0][i].is_aligned = true;

            // align the second sequence to gaps
            m_msa->data[1][i].letter = kGapResidue;
            m_msa->data[1][i].is_aligned = true;
        }

        // Now overwrite the gaps with the aligned sequences
        ITERATE(vector<TAlignedSequence>, itr, aligned_sequence) {
            TAlignedSegment loc = itr->first;  // location in the sequence
            string sequence_data = itr->second;

            for (Uint4 i = loc.first, j = 0; i < loc.second; i++, j++) {
                m_msa->data[1][i].letter = 
                    CSeqportUtil::GetIndex(CSeq_data::e_Ncbistdaa,
                                           sequence_data.substr(j, 1));
            }
        }
   }

   void SetupHenikoffsPositionBasedSequenceWeights(void) {
        const Uint4 kNumAlignedSeqs = 3;    // does not include query
        const Uint1 kQuerySequence[5] = { 7, 22, 19, 7, 17 };
        const Uint1 kSeq1[5] =  { 7,  6,  4, 7,  6 };
        const Uint1 kSeq2[5] =  { 7, 22,  4, 7,  6 };
        const Uint1 kSeq3[5] =  { 7, 22, 15, 7,  7 };

        m_dim.query_length = sizeof(kQuery);
        m_dim.num_seqs = kNumAlignedSeqs;
        m_msa = PSIMsaNew(&m_dim);
        m_query = new unsigned char[sizeof(kQuerySequence)];

        // Initialize aligned sequences
        for (Uint4 s = 0; s < kNumAlignedSeqs; s++) {

            const Uint1* sequence = NULL;
            switch (s) {
            case 0: sequence = kSeq1; break;
            case 1: sequence = kSeq2; break;
            case 2: sequence = kSeq3; break;
            default: abort();    // should never happen
            }

            for (Uint4 i = 0; i < sizeof(kQuerySequence); i++) {
                m_query[i] = kQuerySequence[i];
                m_msa->data[s][i].letter = sequence[i];
                m_msa->data[s][i].is_aligned = true;
            }
        }
   }

   void SetupDuplicateHit(void) {
        const Uint4 kNumAlignedSeqs = 2;    // does not include query

        // This sequence is used as aligned sequence #1 and #2, i.e. it is a
        // duplicate hit
        const Uint1 kGi_129296_[388] = {  
        12,  4, 17,  9, 17, 19, 18, 13,  1, 10,  6,  3,  6,  4, 19, 
         6, 13,  5, 12, 10, 19,  8,  8, 19, 13,  5, 13,  9, 11, 22, 
         3, 14, 11, 17,  9, 11, 18,  1, 11,  1, 12, 19, 22, 11,  7, 
         1, 16,  7, 13, 18,  5, 17, 15, 12, 10, 10, 19, 11,  8,  6, 
         4, 17,  9, 18,  7,  1,  7, 17, 18, 18,  4, 17, 15,  3,  7, 
        17, 17,  5, 22, 19,  8, 13, 11,  6, 10,  5, 11, 11, 17,  5, 
         9, 18, 16, 14, 13,  1, 18, 22, 17, 11,  5,  9,  1,  4, 10, 
        11, 22, 19,  4, 10, 18,  6, 17, 19, 11, 14,  5, 22, 11, 17, 
         3,  1, 16, 10,  6, 22, 18,  7,  7, 19,  5,  5, 19, 13,  6, 
        10, 18,  1,  1,  5,  5,  1, 16, 15, 11,  9, 13, 17, 20, 19, 
         5, 10,  5, 18, 13,  7, 15,  9, 10,  4, 11, 11, 19, 17, 17, 
        17,  9,  4,  6,  7, 18, 18, 12, 19,  6,  9, 13, 18,  9, 22, 
         6, 10,  7,  9, 20, 10,  9,  1,  6, 13, 18,  5,  4, 18, 16, 
         5, 12, 14,  6, 17, 12, 18, 10,  5,  5, 17, 10, 14, 19, 15, 
        12, 12,  3, 12, 13, 13, 17,  6, 13, 19,  1, 18, 11, 14,  1, 
         5, 10, 12, 10,  9, 11,  5, 11, 14, 22,  1, 17,  7,  4, 11, 
        17, 12, 11, 19, 11, 11, 14,  4,  5, 19, 17,  7, 11,  5, 16, 
         9,  5, 10, 18,  9, 13,  6,  4, 10, 11, 16,  5, 20, 18, 17, 
        18, 13,  1, 12,  1, 10, 10, 17, 12, 10, 19, 22, 11, 14, 16, 
        12, 10,  9,  5,  5, 10, 22, 13, 11, 18, 17,  9, 11, 12,  1, 
        11,  7, 12, 18,  4, 11,  6, 17, 16, 17,  1, 13, 11, 18,  7, 
         9, 17, 17, 19,  4, 13, 11, 12,  9, 17,  4,  1, 19,  8,  7, 
        19,  6, 12,  5, 19, 13,  5,  5,  7, 18,  5,  1, 18,  7, 17, 
        18,  7,  1,  9,  7, 13,  9, 10,  8, 17, 11,  5, 11,  5,  5, 
         6, 16,  1,  4,  8, 14,  6, 11,  6,  6,  9, 16, 22, 13, 14, 
        18, 13,  1,  9, 11,  6,  6,  7, 16, 22, 20, 17, 14};

        m_dim.query_length = kQueryLength;
        m_dim.num_seqs = kNumAlignedSeqs;
        m_msa = PSIMsaNew(&m_dim);
        m_query = new unsigned char[kQueryLength];

        for (unsigned int i = 0; i < kQueryLength; i++) {
            m_msa->data[kQueryIndex][i].letter = m_query[i] = kQuery[i];
            m_msa->data[kQueryIndex][i].is_aligned = true;
        }

        for (unsigned int i = 1; i < kNumAlignedSeqs + 1; i++) {
            for (unsigned int j = 0; j < kQueryLength; j++) {
                m_msa->data[i][j].letter = kGi_129296_[j];
                m_msa->data[i][j].is_aligned = true;
            }
        }
   }

   void SetupNearIdenticalHits(void) {
        SetupDuplicateHit();

        const Uint4 kHitIndex = 2;  // index of the near identical hit
        const Uint4 kNumIdenticalResidues = (Uint4) (GetQueryLength() *
            (kPSINearIdentical + 0.01));

        for (Uint4 i = kNumIdenticalResidues; i < GetQueryLength(); i++) {
            Uint1& residue = m_msa->data[kHitIndex][i].letter;
            residue = (residue + 1) % BLASTAA_SIZE;
            BOOST_REQUIRE(residue > 0 && residue < BLASTAA_SIZE);
        }
    }
};

const size_t CPssmInputTestData::kQueryLength;
const Uint1 CPssmInputTestData::kQuery[CPssmInputTestData::kQueryLength] = {
    15,  9, 10,  4, 11, 11, 19, 17, 17, 17, 18,  4, 11,  4, 18, 
    18, 11, 19, 11, 19, 13,  1,  9, 22,  6, 10,  7, 12, 20, 10, 
    18,  1,  6, 13,  1,  5,  4, 18, 16,  5, 12, 14,  6,  8, 19, 
    18, 10, 15,  5, 17, 10, 14, 19, 15, 12, 12,  3, 12, 13, 13, 
    17,  6, 13, 19,  1, 18, 11, 14,  1,  5, 10, 12, 10,  9, 11, 
     5, 11, 14,  6,  1, 17,  7,  4, 11, 17, 12, 11, 19, 11, 11, 
    14,  4,  5, 19, 17,  4, 11,  5, 16,  9,  5, 10, 18,  9, 13, 
     6,  5, 10, 11, 18,  5, 20, 18, 13, 14, 13, 18, 12,  5, 10, 
    16, 16, 19, 10, 19, 22, 11, 14, 15, 12, 10,  9,  5,  5, 10, 
    22, 13, 11, 18, 17, 19, 11, 12,  1, 11,  7, 12, 18,  4, 11, 
     6,  9, 14, 17,  1, 13, 11, 18,  7,  9, 17, 17,  1,  5, 17, 
    11, 10,  9, 17, 15,  1, 19,  8,  7,  1,  6, 12,  5, 11, 17, 
     5,  4,  7,  9,  5, 12,  1,  7, 17, 18,  7, 19,  9,  5,  4, 
     9, 10,  8, 17, 14,  5, 17,  5, 15,  6, 16,  1,  4,  8, 14, 
     6, 11,  6, 11,  9, 10,  8, 13, 14, 18, 13, 18,  9, 19, 22, 
     6,  7, 16, 22, 20, 17, 14};

/// template specializations to automate deallocation of internal BLAST
/// structures with ncbi::AutoPtr
BEGIN_NCBI_SCOPE

// FIXME: declare RAII classes for these?
template <>
struct Deleter<_PSIAlignedBlock> {
    static void Delete(_PSIAlignedBlock* p)
    { _PSIAlignedBlockFree(p); }
};

template <>
struct Deleter<_PSISequenceWeights> {
    static void Delete(_PSISequenceWeights* p)
    { _PSISequenceWeightsFree(p); }
};

template <>
struct Deleter<_PSIInternalPssmData> {
    static void Delete(_PSIInternalPssmData* p)
    { _PSIInternalPssmDataFree(p); }
};

template <>
struct Deleter<_PSIMsa> {
    static void Delete(_PSIMsa* p)
    { _PSIMsaFree(p); }
};

template <>
struct Deleter<_PSIPackedMsa> {
    static void Delete(_PSIPackedMsa* p)
    { _PSIPackedMsaFree(p); }
};

END_NCBI_SCOPE

BOOST_FIXTURE_TEST_SUITE(pssmcreate, CPssmCreateTestFixture)


/// @param query protein sequence in ncbistdaa with sentinel bytes
/// @param query_size length of the query sequence (w/o including sentinel
//  bytes)
static BlastScoreBlk* InitializeBlastScoreBlk(const unsigned char* query,
                                                  Uint4 query_size) {
        const EBlastProgramType kProgramType = eBlastTypeBlastp;
        const double kScaleFactor = 1.0;
        Blast_Message* errors = NULL;
        short status = 0;

        // Setup the scoring options
        CBlastScoringOptions opts;
        status = BlastScoringOptionsNew(kProgramType, &opts);
        BOOST_REQUIRE(status == 0);

        // Setup the sequence block structure
        CBLAST_SequenceBlk query_blk;
        status = BlastSeqBlkNew(&query_blk);
        BOOST_REQUIRE(status == 0);
        status = BlastSeqBlkSetSequence(query_blk, query, query_size);
        BOOST_REQUIRE(status == 0);
        // don't delete the sequences upon exit!
        query_blk->sequence_allocated = FALSE;
        query_blk->sequence_start_allocated = FALSE;

        const Uint1 kNullByte = GetSentinelByte(eBlastEncodingProtein);
        BOOST_REQUIRE(query_blk.Get() != NULL);
        BOOST_REQUIRE(query_blk->sequence[0] != kNullByte);
        BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != kNullByte);
        BOOST_REQUIRE(query_blk->sequence_start[0] == kNullByte);
        BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] ==
                       kNullByte);

        // Setup the query info structure
        CBlastQueryInfo query_info(TestUtil::CreateProtQueryInfo(query_size));

        BlastScoreBlk* retval = NULL;
        status = BlastSetup_ScoreBlkInit(query_blk,
                                          query_info,
                                          opts,
                                          kProgramType,
                                          &retval,
                                          kScaleFactor,
                                          &errors,
                                          &BlastFindMatrixPath);
        if (status) {
            throw runtime_error(errors->message);
        }
        BOOST_REQUIRE(retval->kbp_ideal);

        /*********************************************************************/

        return retval;
}


BOOST_AUTO_TEST_CASE(testFullPssmEngineRunWithDiagnosticsRequest) {

        const string seqalign("data/nr-129295.new.asn.short");
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(seqalign, eSerial_AsnText));

        CRef<CSeq_align_set> sas(new CSeq_align_set());
        *in >> *sas;

        CSeq_id qid("gi|129295"), sid("gi|6");
        auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(qid));
        SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein, q->scope));

        CPSIBlastOptions opts;
        PSIBlastOptionsNew(&opts);

        PSIDiagnosticsRequest request;
        memset((void*) &request, 0, sizeof(request));
        request.information_content = false;        // unsupported
        request.residue_frequencies = true;
        request.weighted_residue_frequencies = true;
        request.frequency_ratios = true;
        request.gapless_column_weights = false; // unsupported

        CRef<IPssmInputData> pssm_strategy(
            new CPsiBlastInputData(seq.data.get()+1,
                                   seq.length-2, // don't count sentinels
                                   sas, q->scope, 
                                   *opts, 
                                   "BLOSUM80",
                                   11,
                                   1,
                                   &request));
        CRef<CPssmEngine> pssm_engine(new CPssmEngine(pssm_strategy));
        CRef<CPssmWithParameters> pssm = pssm_engine->Run();

        const size_t kNumElements = 
            pssm_strategy->GetQueryLength() * BLASTAA_SIZE;
        // Verify the residue frequencies came back
        const CPssmIntermediateData::TResFreqsPerPos& res_freqs =
            pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos();
        BOOST_REQUIRE_EQUAL(kNumElements, res_freqs.size());

        const CPssmIntermediateData::TWeightedResFreqsPerPos& wres_freqs =
            pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos();
        BOOST_REQUIRE_EQUAL(kNumElements, wres_freqs.size());

        const CPssmIntermediateData::TFreqRatios& freq_ratios = 
            pssm->GetPssm().GetIntermediateData().GetFreqRatios();
        BOOST_REQUIRE_EQUAL(kNumElements, freq_ratios.size());

        //TestUtil::PrintTextAsn1Object("pssm-diags.asn", &*pssm);

        // Test the unsupported diagnostics
        pssm_strategy.Reset();
        pssm_engine.Reset();
        memset((void*) &request, 0, sizeof(request));
        request.information_content = true;

        pssm_strategy.Reset(
                new CPsiBlastInputData(seq.data.get(),
                                       seq.length,
                                       sas, q->scope, 
                                       *opts, 
                                       "BLOSUM80",
                                       11,
                                       1,
                                       &request));
        pssm_engine.Reset(new CPssmEngine(pssm_strategy));
        BOOST_CHECK_THROW(pssm_engine->Run(), CBlastException);

        pssm_strategy.Reset();
        pssm_engine.Reset();

        // Test the unsupported diagnostics
        memset((void*) &request, 0, sizeof(request));
        request.gapless_column_weights = true;

        pssm_strategy.Reset(
                new CPsiBlastInputData(seq.data.get(),
                                       seq.length,
                                       sas, q->scope, 
                                       *opts, 
                                       "BLOSUM80",
                                       11,
                                       1,
                                       &request));
        pssm_engine.Reset(new CPssmEngine(pssm_strategy));
        BOOST_CHECK_THROW(pssm_engine->Run(), CBlastException);
}


/// Unit test the individual stages of the PSSM creation algorithm (core
/// layer):
/// 1. purged biased sequences
BOOST_AUTO_TEST_CASE(testPurgeSequencesWithNull) {
        int rv = _PSIPurgeBiasedSegments(NULL);
        BOOST_REQUIRE_EQUAL(PSIERR_BADPARAM, rv);
}

BOOST_AUTO_TEST_CASE(testPurgeSelfHit) {
        auto_ptr<IPssmInputData> pssm_input
            (new CPssmInputTestData(CPssmInputTestData::eSelfHit));
        pssm_input->Process();  // standard calling convention
        AutoPtr<_PSIPackedMsa> msa(_PSIPackedMsaNew(pssm_input->GetData()));
        int rv = _PSIPurgeBiasedSegments(msa.get());
        BOOST_REQUIRE_EQUAL(PSI_SUCCESS, rv);    
        const Uint4 kSelfHitIndex = 1;
        BOOST_REQUIRE_EQUAL(true, (bool) msa->use_sequence[kQueryIndex]);
        BOOST_REQUIRE_EQUAL(false, (bool) msa->use_sequence[kSelfHitIndex]);
}

BOOST_AUTO_TEST_CASE(testPurgeDuplicateHit) {
        auto_ptr<IPssmInputData> pssm_input
            (new CPssmInputTestData(CPssmInputTestData::eDuplicateHit));
        pssm_input->Process();  // standard calling convention
        AutoPtr<_PSIPackedMsa> msa(_PSIPackedMsaNew(pssm_input->GetData()));
        int rv = _PSIPurgeBiasedSegments(msa.get());
        BOOST_REQUIRE_EQUAL(PSI_SUCCESS, rv);    
        const Uint4 kDuplicateHitIndex = 2;
        BOOST_REQUIRE_EQUAL(false, 
                             (bool) msa->use_sequence[kDuplicateHitIndex]);
        BOOST_REQUIRE_EQUAL(true, (bool) msa->use_sequence[kQueryIndex]);
        BOOST_REQUIRE_EQUAL(true, (bool) msa->use_sequence[kQueryIndex + 1]);
}

BOOST_AUTO_TEST_CASE(testPurgeNearIdenticalHits) {
        auto_ptr<IPssmInputData> pssm_input
            (new CPssmInputTestData(CPssmInputTestData::eNearIdenticalHits));
        pssm_input->Process();  // standard calling convention
        AutoPtr<_PSIPackedMsa> msa(_PSIPackedMsaNew(pssm_input->GetData()));
        int rv = _PSIPurgeBiasedSegments(msa.get());
        BOOST_REQUIRE_EQUAL(PSI_SUCCESS, rv);    
        const Uint4 kRemovedHitIndex = 2;
        BOOST_REQUIRE_EQUAL(false, 
                             (bool) msa->use_sequence[kRemovedHitIndex]);
        BOOST_REQUIRE_EQUAL(true, (bool) msa->use_sequence[kQueryIndex]);
        BOOST_REQUIRE_EQUAL(true, (bool) msa->use_sequence[kQueryIndex + 1]);
}

BOOST_AUTO_TEST_CASE(testQueryAlignedWithInternalGaps) {
        auto_ptr<IPssmInputData> pssm_input
            (new CPssmInputTestData
             (CPssmInputTestData::eQueryAlignedWithInternalGaps));
        BOOST_REQUIRE_EQUAL(string("BLOSUM62"),
                             string(pssm_input->GetMatrixName()));
        CPssmEngine pssm_engine(pssm_input.get());
        CRef<CPssmWithParameters> pssm_asn = pssm_engine.Run();

        auto_ptr< CNcbiMatrix<int> > pssm
            (CScorematPssmConverter::GetScores(*pssm_asn));

        /* Make sure that the resulting PSSM's scores are based on the scores
         * of the underlying scoring matrix and the query sequence (i.e.: the
         * PSSM scores should be within one or two values from those in the
         * underlying scoring matrix) */
        
        const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;
        const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
        stringstream ss;
        BOOST_REQUIRE_EQUAL((size_t)pssm_asn->GetPssm().GetNumColumns(),
                             (size_t)pssm->GetCols());
        BOOST_REQUIRE_EQUAL((size_t)pssm_asn->GetPssm().GetNumRows(),
                             (size_t)pssm->GetRows());
        for (int i = 0; i < pssm_asn->GetPssm().GetNumColumns(); i++) {
            for (int j = 0; j < pssm_asn->GetPssm().GetNumRows(); j++) {

                // Exceptional residues get value of BLAST_SCORE_MIN
                if (j == kGapResidue) {
                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM";
                    BOOST_REQUIRE_MESSAGE(BLAST_SCORE_MIN == (*pssm)(j, i), ss.str());
                } else {
                    int score = 
                        (int)NCBISM_GetScore(score_matrix,
                                             pssm_input->GetQuery()[i], j);

                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM: "
                       << "expected=" << NStr::IntToString(score) 
                       << " actual=" << NStr::IntToString((*pssm)(j, i));
                    BOOST_REQUIRE_MESSAGE (score-1 <= (*pssm)(j, i) || (*pssm)(j, i) <= score+1, ss.str());
                }
            }
        }
}
    
BOOST_AUTO_TEST_CASE(testMultiSeqAlignmentHasRegionsUnalignedToQuery) {
        auto_ptr<IPssmInputData> pssm_input
            (new
             CPssmInputTestData(CPssmInputTestData::eMsaHasUnalignedRegion));
        pssm_input->Process();  // standard calling convention
        BOOST_REQUIRE_EQUAL(string("BLOSUM62"),
                             string(pssm_input->GetMatrixName()));


        /*** Run the stage to purge biased alignment segments */
        AutoPtr<_PSIPackedMsa> packed_msa
            (_PSIPackedMsaNew(pssm_input->GetData()));
        int rv = _PSIPurgeBiasedSegments(packed_msa.get());
        BOOST_REQUIRE_EQUAL(PSI_SUCCESS, rv);    
        BOOST_REQUIRE_EQUAL(true, 
                             (bool) packed_msa->use_sequence[kQueryIndex]);
        BOOST_REQUIRE_EQUAL(true, (bool) packed_msa->use_sequence[1]);
        BOOST_REQUIRE_EQUAL(true, (bool) packed_msa->use_sequence[2]);

        AutoPtr<_PSIMsa> msa(_PSIMsaNew(packed_msa.get(), BLASTAA_SIZE));
        /*** Run the stage to calculate alignment extents */
        CPSIBlastOptions opts;
        PSIBlastOptionsNew(&opts);
        AutoPtr<_PSIAlignedBlock> aligned_blocks(
            _PSIAlignedBlockNew(pssm_input->GetQueryLength()));
        rv = _PSIComputeAlignmentBlocks(msa.get(), aligned_blocks.get());
        stringstream ss;
        ss << "_PSIComputeAlignmentBlocks failed: " 
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        // Verify the alignment extents for aligned regions to the query
        vector<CPssmInputTestData::TAlignedSegment> aligned_regions;
        aligned_regions.push_back(make_pair(0U, 99U));
        aligned_regions.push_back(make_pair(200U,
                                            pssm_input->GetQueryLength()-1));

        for (vector<CPssmInputTestData::TAlignedSegment>::const_iterator i =
             aligned_regions.begin();
             i != aligned_regions.end(); ++i) {
            for (TSeqPos pos = i->first; pos < i->second; pos++) {
                ss.str("");
                ss << "Alignment extents differ at position " 
                   << NStr::IntToString(pos);
                BOOST_REQUIRE_MESSAGE((int)i->first == (int)aligned_blocks->pos_extnt[pos].left, ss.str());
                BOOST_REQUIRE_MESSAGE((int)i->second == (int)aligned_blocks->pos_extnt[pos].right, ss.str());
                BOOST_REQUIRE_MESSAGE( (int)(i->second - i->first + 1) == (int)aligned_blocks->size[pos], ss.str());
            }
        }

        // Verify the alignment extents for unaligned regions to the query
        const CPssmInputTestData::TAlignedSegment kUnalignedRange(100, 200); 
        for (size_t i = kUnalignedRange.first; 
             i < kUnalignedRange.second; i++) {
            ss.str("");
            ss << "Alignment extents differ at position " 
               << NStr::IntToString(i);
            BOOST_REQUIRE_MESSAGE((int)-1 == (int)aligned_blocks->pos_extnt[i].left, ss.str());
            BOOST_REQUIRE_MESSAGE( (int)pssm_input->GetQueryLength() == (int)aligned_blocks->pos_extnt[i].right, ss.str());
            BOOST_REQUIRE_MESSAGE(
                (int)(aligned_blocks->pos_extnt[i].right - aligned_blocks->pos_extnt[i].left + 1) == (int)aligned_blocks->size[i],
                ss.str());
        }

        /*** Run the stage to compute the sequence weights */
        blast::TAutoUint1Ptr query_with_sentinels
            (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input->GetQuery(),
                                              pssm_input->GetQueryLength()));;
        CBlastScoreBlk sbp;
        sbp.Reset
            (InitializeBlastScoreBlk
                (query_with_sentinels.get(), pssm_input->GetQueryLength()));
        AutoPtr<_PSISequenceWeights> seq_weights(
            _PSISequenceWeightsNew(msa->dimensions, 
                                   sbp));
        rv = _PSIComputeSequenceWeights(msa.get(), aligned_blocks.get(),
                                        opts->nsg_compatibility_mode,
                                        seq_weights.get());
        ss.str("");
        ss << "_PSIComputeSequenceWeights failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        // Verify the validity of sequence weights corresponding to the aligned
        // regions
        BOOST_REQUIRE_EQUAL(false, (bool)opts->nsg_compatibility_mode);
        const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA[(int)'X'];
        for (vector<CPssmInputTestData::TAlignedSegment>::const_iterator i =
             aligned_regions.begin();
             i != aligned_regions.end(); ++i) {
            for (TSeqPos pos = i->first; pos < i->second; pos++) {
                double total_sequence_weights_for_column = 0.0;
                for (size_t res = 0; res < msa->alphabet_size; res++) {
                    if (res == kXResidue) continue;
                    total_sequence_weights_for_column +=
                        seq_weights->match_weights[pos][res];
                }
                BOOST_REQUIRE(total_sequence_weights_for_column > 0.99 &&
                               total_sequence_weights_for_column < 1.01);
            }
        }
        // Verify that the unaligned sequence weights are all zero's
        for (size_t pos = kUnalignedRange.first; 
             pos < kUnalignedRange.second; pos++) {
            double total_sequence_weights_for_column = 0.0;
            for (size_t res = 0; res < msa->alphabet_size; res++) {
                if (res == kXResidue) continue;
                total_sequence_weights_for_column +=
                    seq_weights->match_weights[pos][res];
            }
            BOOST_REQUIRE(total_sequence_weights_for_column == 0.0);
        }

        /*** run the stage to compute the PSSM's frequency ratios ***/
        AutoPtr<_PSIInternalPssmData> internal_pssm(
            _PSIInternalPssmDataNew(pssm_input->GetQueryLength(), 
                                    sbp->alphabet_size));
        rv = _PSIComputeFreqRatios(msa.get(), seq_weights.get(), sbp,
                                   aligned_blocks.get(), opts->pseudo_count,
                                   opts->nsg_compatibility_mode,
                                   internal_pssm.get());
        ss.str("");
        ss << "_PSIComputeResidueFrequencies failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /***** Run the stage to convert residue frequencies to PSSM **********/
        rv = _PSIConvertFreqRatiosToPSSM(internal_pssm.get(),
                                         msa->query,
                                         sbp,
                                         seq_weights->std_prob);
        ss.str("");
        ss << "_PSIConvertResidueFreqsToPSSM failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /**************** Run the stage to scale the PSSM ********************/
        rv = _PSIScaleMatrix(msa->query,
                             seq_weights->std_prob,
                             internal_pssm.get(),
                             sbp);
        ss.str("");
        ss << "_PSIScaleMatrix failed: " 
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /* Make sure that the resulting PSSM's scores are based on the scores
         * of the underlying scoring matrix and the query sequence (i.e.: the
         * PSSM scores should be within one or two values from those in the
         * underlying scoring matrix) */
        const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;
        const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
        for (Uint4 i = 0; i < pssm_input->GetQueryLength(); i++) {
            for (Uint4 j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                // Exceptional residues get value of BLAST_SCORE_MIN
                if (j == kGapResidue) {
                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM";
                    BOOST_REQUIRE_MESSAGE(BLAST_SCORE_MIN == internal_pssm->pssm[i][j], ss.str());
                } else {
                    int score = 
                        (int)NCBISM_GetScore(score_matrix, msa->query[i], j);

                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM: "
                       << "expected=" << NStr::IntToString(score) 
                       << " actual=" <<
                       NStr::IntToString(internal_pssm->pssm[i][j]);
                    BOOST_REQUIRE_MESSAGE(score-1 <= internal_pssm->pssm[i][j] || internal_pssm->pssm[i][j] <= score+1, ss.str());
                }
            }
        }
}

/// test the case when only a segment of the query sequence is the only
/// aligned sequence in the multiple sequence alignment.
/// The scores in the PSSM should be based on the underlying scoring matrix
BOOST_AUTO_TEST_CASE(testQueryIsOnlyAlignedSequenceInMsa) {
        auto_ptr<IPssmInputData> pssm_input
            (new CPssmInputTestData(CPssmInputTestData::eSelfHit));
        pssm_input->Process();  // standard calling convention
        BOOST_REQUIRE_EQUAL(string("BLOSUM62"),
                             string(pssm_input->GetMatrixName()));


        /*** Run the stage to purge biased alignment segments */
        AutoPtr<_PSIPackedMsa> packed_msa
            (_PSIPackedMsaNew(pssm_input->GetData()));
        int rv = _PSIPurgeBiasedSegments(packed_msa.get());
        BOOST_REQUIRE_EQUAL(PSI_SUCCESS, rv);    
        const Uint4 kSelfHitIndex = 1;
        BOOST_REQUIRE_EQUAL(true, 
                             (bool) packed_msa->use_sequence[kQueryIndex]);
        BOOST_REQUIRE_EQUAL(false, 
                             (bool) packed_msa->use_sequence[kSelfHitIndex]);

        AutoPtr<_PSIMsa> msa(_PSIMsaNew(packed_msa.get(), BLASTAA_SIZE));
        /*** Run the stage to calculate alignment extents */
        CPSIBlastOptions opts;
        PSIBlastOptionsNew(&opts);
        AutoPtr<_PSIAlignedBlock> aligned_blocks(
            _PSIAlignedBlockNew(pssm_input->GetQueryLength()));
        rv = _PSIComputeAlignmentBlocks(msa.get(), aligned_blocks.get());
        stringstream ss;
        ss << "_PSIComputeAlignmentBlocks failed: " 
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        for (size_t i = 0; i < pssm_input->GetQueryLength(); i++) {
            BOOST_REQUIRE_EQUAL((int)-1, 
                                 (int)aligned_blocks->pos_extnt[i].left);
            BOOST_REQUIRE_EQUAL((int)pssm_input->GetQueryLength(),
                                 (int)aligned_blocks->pos_extnt[i].right);
            BOOST_REQUIRE_EQUAL((int)pssm_input->GetQueryLength() + 2,
                                 (int)aligned_blocks->size[i]);
        }

        /*** Run the stage to compute the sequence weights */
        blast::TAutoUint1Ptr query_with_sentinels
            (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input->GetQuery(),
                                              pssm_input->GetQueryLength()));;
        CBlastScoreBlk sbp;
        sbp.Reset
            (InitializeBlastScoreBlk
                (query_with_sentinels.get(), pssm_input->GetQueryLength()));
        AutoPtr<_PSISequenceWeights> seq_weights(
            _PSISequenceWeightsNew(msa->dimensions, 
                                   sbp));
        rv = _PSIComputeSequenceWeights(msa.get(), aligned_blocks.get(),
                                        // N.B.: we're deliberately ignoring
                                        // the sequence weights check!!!!
                                        TRUE,
                                        seq_weights.get());
        ss.str("");
        ss << "_PSIComputeSequenceWeights failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /*** run the stage to compute the PSSM's frequency ratios ***/
        AutoPtr<_PSIInternalPssmData> internal_pssm(
            _PSIInternalPssmDataNew(pssm_input->GetQueryLength(), 
                                    sbp->alphabet_size));
        rv = _PSIComputeFreqRatios(msa.get(), seq_weights.get(), sbp,
                                   aligned_blocks.get(), opts->pseudo_count,
                                   opts->nsg_compatibility_mode,
                                   internal_pssm.get());
        ss.str("");
        ss << "_PSIComputeResidueFrequencies failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /***** Run the stage to convert residue frequencies to PSSM **********/
        rv = _PSIConvertFreqRatiosToPSSM(internal_pssm.get(),
                                         msa->query,
                                         sbp,
                                         seq_weights->std_prob);
        ss.str("");
        ss << "_PSIConvertResidueFreqsToPSSM failed: "
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /**************** Run the stage to scale the PSSM ********************/
        rv = _PSIScaleMatrix(msa->query,
                             seq_weights->std_prob,
                             internal_pssm.get(),
                             sbp);
        ss.str("");
        ss << "_PSIScaleMatrix failed: " 
           << CPssmCreateTestFixture::x_ErrorCodeToString(rv);
        BOOST_REQUIRE_MESSAGE(PSI_SUCCESS == rv, ss.str());

        /* Make sure that the resulting PSSM's scores are based on the scores
         * of the underlying scoring matrix and the query sequence (i.e.: the
         * PSSM scores should be within one or two values from those in the
         * underlying scoring matrix) */
        const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;
        const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
        for (Uint4 i = 0; i < pssm_input->GetQueryLength(); i++) {
            for (Uint4 j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                // Exceptional residues get value of BLAST_SCORE_MIN
                if (j == kGapResidue) {
                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM";
                    BOOST_REQUIRE_MESSAGE(BLAST_SCORE_MIN == internal_pssm->pssm[i][j], ss.str());
                } else {
                    int score = 
                        (int)NCBISM_GetScore(score_matrix, msa->query[i], j);

                    ss.str("");
                    ss << "Position " << i << " residue " 
                       << TestUtil::GetResidue(j) << " differ on PSSM: "
                       << "expected=" << NStr::IntToString(score) 
                       << " actual=" <<
                       NStr::IntToString(internal_pssm->pssm[i][j]);
                    BOOST_REQUIRE_MESSAGE(score-1 <= internal_pssm->pssm[i][j] || internal_pssm->pssm[i][j] <= score+1, ss.str());
                }
            }
        }
}

BOOST_AUTO_TEST_CASE(testRejectFlankingGaps) {
        auto_ptr<IPssmInputData> bad_pssm_data(new CPssmInputFlankingGaps());
        CPssmEngine pssm_engine(bad_pssm_data.get());
        BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}

BOOST_AUTO_TEST_CASE(testRejectGapInQuery) {
        auto_ptr<IPssmInputData> bad_pssm_data(new CPssmInputGapsInQuery());
        CPssmEngine pssm_engine(bad_pssm_data.get());
        BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}

BOOST_AUTO_TEST_CASE(testRejectQueryLength0) {
        auto_ptr<IPssmInputData> bad_pssm_data(new CPssmInputQueryLength0());
        BOOST_REQUIRE_THROW(CPssmEngine pssm_engine(bad_pssm_data.get()), CPssmEngineException);
}

BOOST_AUTO_TEST_CASE(testRejectNullPssmInputData) {
        IPssmInputData* null_ptr = NULL;
        BOOST_REQUIRE_THROW(CPssmEngine pssm_engine(null_ptr), CPssmEngineException);
}

BOOST_AUTO_TEST_CASE(testRejectNullsReturnedByPssmInput) {
        auto_ptr<IPssmInputData> bad_pssm_data(new CNullPssmInput());
         BOOST_REQUIRE_THROW(CPssmEngine pssm_engine(bad_pssm_data.get()), CBlastException);
}

BOOST_AUTO_TEST_CASE(testRejectUnsupportedMatrix) {
        auto_ptr<IPssmInputData> bad_pssm_data(new
                                               CPssmInputUnsupportedMatrix());
        BOOST_REQUIRE_THROW(CPssmEngine pssm_engine(bad_pssm_data.get()), CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: pssmcreate-cppunit.cpp,v $
* Revision 1.86  2008/03/13 19:41:58  camacho
* Bring up to date with current CScorematPssmConverter interface
*
* Revision 1.85  2007/12/07 17:19:17  camacho
* Bring in sync with svn revision 115203
*
* Revision 1.84  2007/04/10 18:24:36  madden
* Remove discontinuous seq-aligns
*
* Revision 1.83  2007/01/23 18:02:19  camacho
* + new parameter to posPurgeMatches
*
* Revision 1.82  2006/11/17 17:58:01  camacho
* Update to use new definition of CPsiBlastInputData::x_GetSubjectSequence
*
* Revision 1.81  2006/11/16 14:06:20  camacho
* Add missing Deleter specialization
*
* Revision 1.80  2006/11/14 15:56:41  camacho
* Bring up to date with most recent PSSM engine optimizations
*
* Revision 1.79  2006/08/31 22:04:52  camacho
* Minor fix
*
* Revision 1.78  2006/07/05 15:24:15  camacho
* Changes to support new value of BLASTAA_SIZE
*
* Revision 1.77  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.76  2006/05/24 17:22:43  madden
* remove call to FindMatrixPath
*
* Revision 1.75  2006/04/26 14:24:47  camacho
* Fix compiler warning
*
* Revision 1.74  2006/02/21 22:10:15  camacho
* Use CNcbiOstrstream and CNcbiOstrstreamToString
*
* Revision 1.73  2006/02/17 18:50:38  camacho
* Replace ostringstream for CNcbiOstrstream for portability issues
*
* Revision 1.72  2006/01/30 17:30:34  camacho
* Relax the maximum permissible difference when comparing doubles
*
* Revision 1.71  2005/11/28 20:46:04  camacho
* Fixes to temporary BLAST object manager class to create CScopes
*
* Revision 1.70  2005/11/10 23:43:31  camacho
* Use TestUtil::CTmpObjMgrBlastDbDataLoader
*
* Revision 1.69  2005/10/26 14:30:46  camacho
* Remove redundant code, reuse private PSI-BLAST auxiliary functions
*
* Revision 1.68  2005/10/14 13:47:32  camacho
* Fixes to pacify icc compiler
*
* Revision 1.67  2005/09/26 16:35:15  camacho
* Use CRef<> to store CPssmEngine
*
* Revision 1.66  2005/09/26 14:41:44  camacho
* Renamed blast_psi.hpp -> pssm_engine.hpp
*
* Revision 1.65  2005/09/23 18:59:11  camacho
* Rollback accidental commit
*
* Revision 1.63  2005/08/26 17:14:06  camacho
* Remove unneeded typedefs
*
* Revision 1.62  2005/08/24 14:46:48  camacho
* Updated tests for PSSM engine
*
* Revision 1.61  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.60  2005/05/20 18:33:20  camacho
* refactorings to use CAsn1PssmConverter
*
* Revision 1.59  2005/05/10 16:09:04  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.58  2005/05/04 13:28:38  camacho
* Fix to previous commit
*
* Revision 1.57  2005/05/03 20:45:07  camacho
* Added test for query aligned with gaps
*
* Revision 1.56  2005/04/29 14:44:53  bealer
* - Fix for inverted test in DOUBLES_EQUAL_MSG (required for release mode).
*
* Revision 1.55  2005/04/27 20:08:40  dondosha
* PHI-blast boolean argument has been removed from BlastSetup_ScoreBlkInit
*
* Revision 1.54  2005/04/22 13:32:13  camacho
* Fix to previous commit
*
* Revision 1.53  2005/04/21 20:45:58  camacho
* Added test for the case when the query sequence is aligned with internal gaps only on a given column
*
* Revision 1.52  2005/03/23 14:27:00  camacho
* Fix compiler warnings
*
* Revision 1.51  2005/03/22 15:47:50  camacho
* added tests for backwards compatibility with old PSSM engine
*
* Revision 1.50  2005/03/21 23:34:44  bealer
* - Doubles/message macro.
*
* Revision 1.49  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.48  2005/03/03 17:45:58  camacho
* fix to loading pssm
*
* Revision 1.47  2005/02/25 19:48:14  camacho
* Added unit test for comparing new vs. old IMPALA scaling
*
* Revision 1.46  2005/02/22 22:51:20  camacho
* + impala_scaling_factor, first cut
*
* Revision 1.45  2005/02/14 14:17:17  camacho
* Changes to use SBlastScoreMatrix
*
* Revision 1.44  2005/02/10 15:43:28  dondosha
* Small memory leak fix
*
* Revision 1.43  2005/01/26 17:52:13  camacho
* Remove unused variables
*
* Revision 1.42  2005/01/22 16:57:01  camacho
* cosmetic change
*
* Revision 1.41  2005/01/10 15:43:52  camacho
* + data/seqp database to database loader
*
* Revision 1.40  2004/12/28 16:48:26  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.39  2004/12/22 16:26:56  camacho
* Remove diagnostics output
*
* Revision 1.38  2004/12/13 22:37:56  camacho
* Consolidated structure group customizations in option: nsg_compatibility_mode
*
* Revision 1.37  2004/12/09 15:24:10  dondosha
* BlastSetup_GetScoreBlock renamed to BlastSetup_ScoreBlkInit
*
* Revision 1.36  2004/11/30 20:43:38  camacho
* Replace call to GetLoaderNameFromArgs
*
* Revision 1.35  2004/11/29 20:18:03  camacho
* Fix setUp/tearDown methods to avoid creating/deleting the Genbank data loader
* as this spawns many maintenance threads and causes valgrind to fail.
*
* Revision 1.34  2004/11/24 15:16:58  camacho
* + test for default PSIBLAST input data strategy
*
* Revision 1.33  2004/11/23 21:50:08  camacho
* Removed local initialization of ideal Karlin-Altschul parameters
*
* Revision 1.32  2004/11/23 17:53:18  camacho
* Return NULL rather than "" in null matrix test case
*
* Revision 1.31  2004/11/22 15:18:13  camacho
* + tests & mock object for purge stage of PSSM creation
*
* Revision 1.30  2004/11/02 21:27:22  camacho
* Fixes for recent changes in PSI-BLAST function names
*
* Revision 1.29  2004/10/18 14:51:49  camacho
* Added argument to _PSIComputeSequenceWeights
*
* Revision 1.28  2004/10/13 20:49:22  camacho
* + support for requesting diagnostics information and specifying underlying matrix
*
* Revision 1.27  2004/10/13 15:46:23  camacho
* + tests for invalid PSSM data
*
* Revision 1.26  2004/10/13 01:43:54  camacho
* + unit test for checking 0-length queries
*
* Revision 1.25  2004/10/12 21:27:49  camacho
* + mock objects to simulate bad pssm input data
*
* Revision 1.24  2004/10/12 14:19:36  camacho
* Update for scoremat.asn reorganization
*
* Revision 1.23  2004/08/31 16:10:07  camacho
* Use CppUnit assertions for floating point values
*
* Revision 1.22  2004/08/05 19:20:27  camacho
* Temporarily disable failing test
*
* Revision 1.21  2004/08/04 21:20:55  camacho
* Change seq-align file
*
* Revision 1.20  2004/08/04 20:28:49  camacho
* Updated to reflect recent changes in core PSSM engine structures
*
* Revision 1.19  2004/08/02 13:31:28  camacho
* Renaming of PSSM engine structures
*
* Revision 1.18  2004/07/29 17:56:12  camacho
* Updated to use new interfaces, needs more test data
*
* Revision 1.17  2004/07/22 16:37:59  camacho
* Fixes for exchanging data loaders
*
* Revision 1.16  2004/07/22 13:58:59  camacho
* Use the new C++ Object Manager interfaces
*
* Revision 1.15  2004/07/21 17:51:03  camacho
* disable failing unit tests for right now
*
* Revision 1.14  2004/07/07 18:55:38  camacho
* Add test for handling out-of-memory conditions
*
* Revision 1.13  2004/07/06 15:58:45  dondosha
* Use EBlastProgramType enumeration type for program when calling C functions
*
* Revision 1.12  2004/07/02 18:02:54  camacho
* Added more tests for purging matching sequences and sequence weights
* computation.
*
* Revision 1.11  2004/06/22 16:46:19  camacho
* Changed the blast_type_* definitions for the EBlastProgramType enumeration.
*
* Revision 1.10  2004/06/21 15:51:34  camacho
* Added compute extents tests, fixed memory leaks
*
* Revision 1.9  2004/06/18 15:05:34  camacho
* Added more comparison tests
*
* Revision 1.8  2004/06/16 15:23:48  camacho
* Added posPurgeMatches unit tests
*
* Revision 1.7  2004/06/16 12:48:26  camacho
* Fix compiler warnings
*
* Revision 1.6  2004/06/16 12:12:47  camacho
* Remove extra comma in enumerated type
*
* Revision 1.5  2004/06/14 21:33:49  camacho
* Refactored test code to use a pssm engine mock object
*
* Revision 1.4  2004/06/09 21:34:20  camacho
* Minor changes
*
* Revision 1.3  2004/06/09 16:45:17  camacho
* Fix for solaris build
*
* Revision 1.2  2004/06/09 16:17:29  camacho
* Minor fixes
*
* Revision 1.1  2004/06/09 14:58:55  camacho
* Initial revision
*
*
* ===========================================================================
*/
