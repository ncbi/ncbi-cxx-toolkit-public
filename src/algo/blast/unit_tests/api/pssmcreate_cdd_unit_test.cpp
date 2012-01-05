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
 * Author:  Greg Boratyn
 *
 */

/** @file pssmcreate_cdd_unit_test.cpp
 * Unit test module for creation of PSSMs from multiple alignments of
   conserved domains.
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

// Serial library includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

// Object includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>

// ASN.1 definition for PSSM (scoremat)
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>

// BLAST includes
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/cdd_pssm_input.hpp>
#include <algo/blast/api/rps_aux.hpp> // for CBlastRPSInfo
#include <blast_objmgr_priv.hpp>
#include <blast_psi_priv.h>
#include "psiblast_aux_priv.hpp"    // for CScorematPssmConverter

// Unit test auxiliary includes
#include "blast_test_util.hpp"
#include "pssm_test_util.hpp"
#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;


/// Query id used for tests
static const string kQueryId = "gi|129295";

/// Domain subject id used for tests (present in test CDD)
static const string kSubjectId = "gnl|CDD|29117";

/// Class for testing methods of CCddInputData class.
/// The class creates instances of CCddInputData and accesses CCddInputData 
/// private attributes and methods.
class CPssmCddInputTest
{
public:

    /// Type of test multiple alignment of CDs
    enum EType {
        /// Duplicate CDD hit
        eDuplicateOverlappingHit,
        /// Duplicate CDD hit that
        /// does not intersect query range
        eDuplicateNonOverlappingHit,
    };

    /// Create CCddInputData test object with given CD alignment
    /// @param type Alignment type
    /// @return Pssm strategy
    static CRef<CCddInputData> CreatePssmInput(EType type);

    /// Get number of CDs in the internal MSA
    static size_t GetMsaSize(const CCddInputData& input);

    /// Get alphabet size used by CCddInputData
    static int GetAlphabetSize(void);

    /// Get the scale factor for residue frequencis and independent
    /// observations stored in CDD
    static int GetProfilesDataScale(void);

    /// Find index of a CD in MSA by Seq_id
    /// @param input PSSM strategy object
    /// @param subject Seq_id of the CD
    /// @return Index of subject in MSA or -1 if subject not found in MSA
    static int GetSubjectMsaIndex(const CCddInputData& input,
                                  const CSeq_id& subject);

    /// Get number of CDD hits in CCddInputData object
    static size_t GetNumHits(const CCddInputData& input);

    /// Call CCddInputData method that removes multiple CD hits
    static void RemoveMultipleCdHits(CCddInputData& input);

    /// Create a dummy alignment with one segment
    /// @param query_id Query id
    /// @param subject_id Subject id
    /// @param qfrom Query start position
    /// @param sfrom Subject start position
    /// @param len Alignment length
    /// @return Seq_align object
    static CRef<CSeq_align> x_CreateAlignment(CRef<CSeq_id> query_id,
                                              CRef<CSeq_id> subject_id,
                                              int qfrom, int sfrom, int len);

    /// Create two alignments with the same CD that overlap
    static CRef<CSeq_align_set> x_CreateDuplicateOverlappingHit(
                                                CRef<CSeq_id> query_id);


    /// Create two alignment with the same CD that do not overlap
    static CRef<CSeq_align_set> x_CreateDuplicateNonOverlappingHit(
                                                CRef<CSeq_id> query_id);
};

// A series of classes for testing PSSM computation on the core level

/// Simple PSSM computation strategy with one CD
class CPssmInputWithNoCDs : public IPssmInputCdd
{
public:

    CPssmInputWithNoCDs(void);

    virtual ~CPssmInputWithNoCDs() {}

    virtual PSICdMsa* GetData(void) {return &m_CdMsa;}

    virtual const PSIBlastOptions* GetOptions(void) {return m_Options;}

    virtual void Process(void) {}

    virtual unsigned char* GetQuery(void) {return &m_Query[0];}

    virtual unsigned int GetQueryLength(void) {return kQueryLength;}


protected:

    PSICdMsa m_CdMsa;
    PSIMsaDimensions m_Dimensions;
    CPSIBlastOptions m_Options;
    vector<unsigned char> m_Query;
    vector<PSICdMsaCell*> m_Msa;

    static const int kQueryLength = 6;
};


/// Simple PSSM computation strategy with one CD
class CPssmInputWithSingleCD : public CPssmInputWithNoCDs
{
public:
    CPssmInputWithSingleCD(void);

    virtual ~CPssmInputWithSingleCD();    

protected:
    vector<double> m_Freqs;
    vector<PSICdMsaCellData> m_MsaData;
};

/// Simple PSSM computation strategy with two CDs
class CPssmInputWithTwoCDs : public CPssmInputWithSingleCD
{
public:
    CPssmInputWithTwoCDs(void);

    virtual ~CPssmInputWithTwoCDs() {}
};

/// PSSM computation strategy with gaps in query
class CPssmInputWithGapsInQuery : public CPssmInputWithSingleCD
{
public:
    CPssmInputWithGapsInQuery(void) {
        _ASSERT(m_CdMsa.dimensions->num_seqs > 0);
        _ASSERT(m_CdMsa.msa[0][0].data);
        _ASSERT(m_CdMsa.msa[0][0].data->wfreqs);

        m_CdMsa.query[1] = AMINOACID_TO_NCBISTDAA[(int)'-'];
    }
};

/// PSSM computation strategy with domains with negative residue frequencies
class CPssmInputWithNegativeFreqs : public CPssmInputWithSingleCD
{
public:
    CPssmInputWithNegativeFreqs(void) {
        _ASSERT(m_CdMsa.dimensions->num_seqs > 0);
        _ASSERT(m_CdMsa.msa[0][0].data);
        _ASSERT(m_CdMsa.msa[0][0].data->wfreqs);

        const Uint1 kResidueA = AMINOACID_TO_NCBISTDAA[(int)'A'];
        const Uint1 kResidueC = AMINOACID_TO_NCBISTDAA[(int)'C'];

        m_CdMsa.msa[0][0].data->wfreqs[kResidueA] = -0.001;
        m_CdMsa.msa[0][0].data->wfreqs[kResidueC] += 0.001;
    }
};

/// PSSM computation strategy with domains with frequencies that do not sum
/// to 1
class CPssmInputWithUnnormalizedFreqs : public CPssmInputWithSingleCD
{
public:
    CPssmInputWithUnnormalizedFreqs(void) {
        _ASSERT(m_CdMsa.dimensions->num_seqs > 0);
        _ASSERT(m_CdMsa.msa[0][0].data);
        _ASSERT(m_CdMsa.msa[0][0].data->wfreqs);

        const Uint1 kResidueA = AMINOACID_TO_NCBISTDAA[(int)'A'];

        m_CdMsa.msa[0][0].data->wfreqs[kResidueA] += 0.01;
    }
};

/// PSSM computation strategy with domains with zero observations
class CPssmInputWithZeroObservations : public CPssmInputWithSingleCD
{
public:
    CPssmInputWithZeroObservations(void) {
        _ASSERT(m_CdMsa.dimensions->num_seqs > 0);
        _ASSERT(m_CdMsa.msa[0][0].data);
        _ASSERT(m_CdMsa.msa[0][0].data->wfreqs);

        m_CdMsa.msa[0][0].data->iobsr = 0.0;
    }
};


// Test computing frequency ratios and PSSM scores
static void s_TestCreatePssmFromFreqs(const PSICdMsa* cd_msa,
                                      CBlastScoreBlk& sbp,
                                      const PSIBlastOptions* opts,
                                      AutoPtr<_PSISequenceWeights>& seq_weights);



BOOST_AUTO_TEST_SUITE(pssmcreate_cdd)


// Tests for code in algo/blast/core for computing PSSM from conserved domains

// Tests for pre and post conditions for computing residue frequencies,
// frequency ratios and PSSM scores
BOOST_AUTO_TEST_CASE(TestCreatePssmFromSingleCd)
{
    // create pssm input with a single CD in the alignment
    CPssmInputWithSingleCD pssm_input;

    blast::TAutoUint1Ptr query_with_sentinels
        (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input.GetQuery(),
                                             pssm_input.GetQueryLength()));


    PSICdMsa* cd_msa = pssm_input.GetData();
    CBlastScoreBlk sbp;
    sbp.Reset(InitializeBlastScoreBlk(query_with_sentinels.get(),
                                      pssm_input.GetQueryLength()));
    
    const PSIBlastOptions* opts = pssm_input.GetOptions();

    AutoPtr<_PSISequenceWeights> seq_weights(_PSISequenceWeightsNew(
                                                    cd_msa->dimensions, sbp));

    // compute and verify residue frequencies

    // verify that that the function returns success
    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, sbp.Get(), opts,
                                                seq_weights.get()) == 0);

    // verify that for a pssm input with a single CD hit residue frequencies
    // and number of indpendent observations are the same as in domain model
    for (int i=0;i < (int)cd_msa->dimensions->query_length;i++) {
        if (cd_msa->msa[0][i].is_aligned) {
            BOOST_REQUIRE_CLOSE(seq_weights->independent_observations[i],
                                cd_msa->msa[0][i].data->iobsr,
                                1e-5);

            for (int j=0;j < (int)sbp->alphabet_size;j++) {
                BOOST_REQUIRE_CLOSE(seq_weights->match_weights[i][j],
                                    cd_msa->msa[0][i].data->wfreqs[j],
                                    1e-5);
            }
        }
    }

    s_TestCreatePssmFromFreqs(cd_msa, sbp, opts, seq_weights);
}


BOOST_AUTO_TEST_CASE(TestCreatePssmFromMultipleCds)
{
    // create pssm input with two CD in the alignment
    CPssmInputWithTwoCDs pssm_input;

    blast::TAutoUint1Ptr query_with_sentinels
        (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input.GetQuery(),
                                             pssm_input.GetQueryLength()));


    PSICdMsa* cd_msa = pssm_input.GetData();
    CBlastScoreBlk sbp;
    sbp.Reset(InitializeBlastScoreBlk(query_with_sentinels.get(),
                                      pssm_input.GetQueryLength()));
    
    const PSIBlastOptions* opts = pssm_input.GetOptions();

    AutoPtr<_PSISequenceWeights> seq_weights(_PSISequenceWeightsNew(
                                                    cd_msa->dimensions, sbp));

    // verify that computing frequencies finishes with success
    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, sbp.Get(), opts,
                                                seq_weights.get()) == 0);

    for (int i=0;i < (int)cd_msa->dimensions->query_length;i++) {

        // verify that CDs are aligned to query in each column
        BOOST_REQUIRE(cd_msa->msa[0][i].is_aligned
                      && cd_msa->msa[1][i].is_aligned);

        // verify that number of observations is the same as the sum of
        // observations from each CD in the alignment
        BOOST_REQUIRE_CLOSE(seq_weights->independent_observations[i],
                            cd_msa->msa[0][i].data->iobsr
                            + cd_msa->msa[1][i].data->iobsr,
                            1e-5);

        // verify that residue frequencies sum to 1
        double sum = 0.0;
        for (int j=0;j < (int)sbp->alphabet_size;j++) {
            sum += seq_weights->match_weights[i][j];
        }
        BOOST_REQUIRE_CLOSE(sum, 1.0, 1e-5);
    }

    s_TestCreatePssmFromFreqs(cd_msa, sbp, opts, seq_weights);
}

BOOST_AUTO_TEST_CASE(TestCreatePssmFromNoCds)
{
    // create pssm input with no CDs in the alignment
    CPssmInputWithNoCDs pssm_input;
    
    blast::TAutoUint1Ptr query_with_sentinels
        (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input.GetQuery(),
                                             pssm_input.GetQueryLength()));


    PSICdMsa* cd_msa = pssm_input.GetData();
    CBlastScoreBlk sbp;
    sbp.Reset(InitializeBlastScoreBlk(query_with_sentinels.get(),
                                      pssm_input.GetQueryLength()));
    
    const PSIBlastOptions* opts = pssm_input.GetOptions();

    AutoPtr<_PSISequenceWeights> seq_weights(_PSISequenceWeightsNew(
                                                    cd_msa->dimensions, sbp));

    // compute and verify residue frequencies

    // verify that that the function returns success
    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, sbp.Get(), opts,
                                                seq_weights.get()) == 0);

    // verify computation of frequency ratios and pssm scores
    s_TestCreatePssmFromFreqs(cd_msa, sbp, opts, seq_weights);
}

// Verify that CdMsa with gaps in query returns error
BOOST_AUTO_TEST_CASE(TestRejectGapsInQuery)
{
    CPssmInputWithGapsInQuery pssm_input;

    CPssmEngine pssm_engine(&pssm_input);
    BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}


BOOST_AUTO_TEST_CASE(TestRejectDomainsWithNegativeFreqs)
{
    CPssmInputWithNegativeFreqs pssm_input;

    CPssmEngine pssm_engine(&pssm_input);
    BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}
 
BOOST_AUTO_TEST_CASE(TestRejectDomainsWithUnnormalizedFreqs)
{
    CPssmInputWithUnnormalizedFreqs pssm_input;

    CPssmEngine pssm_engine(&pssm_input);
    BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}

BOOST_AUTO_TEST_CASE(TestRejectDomainsWithZeroObservations)
{
    CPssmInputWithZeroObservations pssm_input;

    CPssmEngine pssm_engine(&pssm_input);
    BOOST_REQUIRE_THROW(pssm_engine.Run(), CBlastException);
}


BOOST_AUTO_TEST_CASE(TestRejectNullInput)
{
    CPssmInputWithSingleCD pssm_input;

    blast::TAutoUint1Ptr query_with_sentinels
        (CPssmCreateTestFixture::x_GuardProteinQuery(pssm_input.GetQuery(),
                                             pssm_input.GetQueryLength()));


    PSICdMsa* cd_msa = pssm_input.GetData();
    CBlastScoreBlk sbp;
    sbp.Reset(InitializeBlastScoreBlk(query_with_sentinels.get(),
                                      pssm_input.GetQueryLength()));
    
    // set default options
    const PSIBlastOptions* opts = pssm_input.GetOptions();

    AutoPtr<_PSISequenceWeights> seq_weights(_PSISequenceWeightsNew(
                                                    cd_msa->dimensions, sbp));
        
    // verify that an error code is returned for missing argument
    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(NULL, sbp.Get(), opts,
                                                seq_weights.get()));

    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, NULL, opts,
                                                seq_weights.get()));

    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, sbp.Get(), NULL,
                                                seq_weights.get()));

    BOOST_REQUIRE(_PSIComputeFrequenciesFromCDs(cd_msa, sbp.Get(), opts, NULL));

    Int4 pseudo_count = 0;
    AutoPtr<_PSIInternalPssmData> internal_pssm(_PSIInternalPssmDataNew(
                                           cd_msa->dimensions->query_length,
                                           (Uint4)sbp->alphabet_size));

    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(NULL, seq_weights.get(),
                                               sbp.Get(), pseudo_count,
                                               internal_pssm.get()));

    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(cd_msa, NULL, sbp.Get(),
                                               pseudo_count,
                                               internal_pssm.get()));

    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(cd_msa, seq_weights.get(), NULL,
                                               pseudo_count,
                                               internal_pssm.get()));

    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(cd_msa, seq_weights.get(),
                                               sbp.Get(), -1,
                                               internal_pssm.get()));

    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(cd_msa, seq_weights.get(),
                                               sbp.Get(), pseudo_count, NULL));
}


//---------------------------------------------------------------------



// Tests for CCddInputData class -- strategy for computing PSSM from CDD hits

// Verify that CDD search results are correctly converted to multiple alignment
// of CDs
BOOST_AUTO_TEST_CASE(TestConvertSeqalignToCdMsa)
{
        const string seqalign("data/cdd-129295.asn");
        const string rpsdb("data/deltatest");
        
        /*** Setup code ***/
        CRef<CSeq_id> qid(new CSeq_id("gi|129295"));
        auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(*qid));

        // read alignments
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(seqalign, eSerial_AsnText));

        CRef<CSeq_align_set> sas(new CSeq_align_set());
        *in >> *sas;
        BOOST_REQUIRE(sas->Get().size() != 0);

        CPSIBlastOptions opts;
        PSIBlastOptionsNew(&opts);

        // retrieve the query sequence, but skip the sentinel bytes
        SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein,
                                       q->scope));

        try {
            // create pssm engine strategy
            CRef<CCddInputData> pssm_input(new CCddInputData(seq.data.get() + 1, 
                                                             seq.length - 2,
                                                             sas, 
                                                             *opts,
                                                             rpsdb));

            pssm_input->Process();

            // open CDD database for checking residue frequencies and
            // indepedent observations
            CBlastRPSInfo profile_data(rpsdb, CBlastRPSInfo::fDeltaBlast);

            // verify that CDD was open properly
            BOOST_REQUIRE(profile_data()->freq_header);
            BOOST_REQUIRE(profile_data()->obsr_header);
            BOOST_REQUIRE_EQUAL(profile_data()->freq_header->num_profiles,
                                profile_data()->obsr_header->num_profiles);

            // get residue freqs from CDD
            BlastRPSProfileHeader* freq_header = profile_data()->freq_header;
            int kNumDomains = freq_header->num_profiles;

            const Int4* freq_offsets = freq_header->start_offsets;
            const CCddInputData::TFreqs* freq_start =
                (CCddInputData::TFreqs*)(freq_header->start_offsets
                                         + kNumDomains + 1);

            // get independent observations from CDD
            BlastRPSProfileHeader* obsr_header = profile_data()->obsr_header;
            const Int4* obsr_offsets = obsr_header->start_offsets;
            const CCddInputData::TObsr* obsr_start =
                (CCddInputData::TObsr*)obsr_header->start_offsets
                + kNumDomains + 1;

            CSeqDB seqdb(rpsdb, CSeqDB::eProtein);


            /*** End Setup code ***/
            

            // Walk through the alignment segments and ensure that PSICdMsa
            // is filled properly

            // TO DO: Make sure that each subject appears once in the
            // seq-align-set

            const PSICdMsa* cd_msa = pssm_input->GetData();

            // verify query length in msa
            BOOST_REQUIRE_EQUAL(cd_msa->dimensions->query_length,
                                seq.length-2);

            // verify number of subjects
            BOOST_REQUIRE_EQUAL(cd_msa->dimensions->num_seqs,
                                sas->Get().size());

            // verify that msa size is the same as provided by the
            // cd_msa->dimensions structure
            BOOST_REQUIRE_EQUAL(cd_msa->dimensions->query_length
                                * cd_msa->dimensions->num_seqs,
                        CPssmCddInputTest::GetMsaSize(*pssm_input));

            // verify query sequence in msa
            for (int i=0;i < (int)seq.length-2;i++) {
                BOOST_REQUIRE_EQUAL((Uint1)cd_msa->query[i],
                                    seq.data.get()[i + 1]);
            }
            
            const int kAlphabetSize = CPssmCddInputTest::GetAlphabetSize();
            const int kScale = CPssmCddInputTest::GetProfilesDataScale();


            ITERATE (CSeq_align_set::Tdata, hsp, sas->Get()) {
                const CDense_seg& ds = (*hsp)->GetSegs().GetDenseg();
                BOOST_REQUIRE_EQUAL(ds.GetDim(), 2);
                const CSeq_id& subject = ds.GetSeq_id(1);
                const vector<TSignedSeqPos>& starts = ds.GetStarts();
                const vector<TSeqPos>& lengths = ds.GetLens();
                
                // get subject domain database ordinal id
                int db_oid;
                seqdb.SeqidToOid(subject, db_oid);
                BOOST_REQUIRE(db_oid >= 0 && db_oid < kNumDomains);

                // get subject frequency data from CDD
                const CCddInputData::TFreqs* freqs = 
                    freq_start + freq_offsets[db_oid] * kAlphabetSize;

                // get subject observations data from CDD
                const CCddInputData::TObsr* obsr_c =
                    obsr_start + obsr_offsets[db_oid];

                // decompress independent observations
                int obsr_size =
                    obsr_offsets[db_oid + 1] - obsr_offsets[db_oid];
                vector<CCddInputData::TObsr> obsr;
                for (int i=0;i < obsr_size;i+=2) {
                    CCddInputData::TObsr val = obsr_c[i];
                    Int4 num = (Int4)obsr_c[i + 1];
                    
                    for (int j=0;j < num;j++) {
                        obsr.push_back(val);
                    }
                }

                // get subject index in CdMSA
                int msa_index = CPssmCddInputTest::GetSubjectMsaIndex(
                                                                   *pssm_input,
                                                                   subject);

                // verify that that subject index is sane
                BOOST_REQUIRE(msa_index >= 0
                              && msa_index < (int)sas->Get().size());

                // walk through alignment segments
                int k = 0;
                const int kGap = -1;
                for (int i=0;i < ds.GetNumseg(); i++) {
                    TSignedSeqPos q_index = starts[i*ds.GetDim()];
                    TSignedSeqPos s_index = starts[i*ds.GetDim()+1];

                    // verify that segments not present in denseg
                    // are marked as not aligned in MSA
                    while (k < q_index) {
                        BOOST_REQUIRE_EQUAL(cd_msa->msa[msa_index][k].is_aligned,
                                            (Uint1)false);
                        k++;
                    }

                    if (s_index == kGap) {

                        // verify that deletions in subject are marked as
                        // not aligned in MSA
                        for (TSeqPos pos = 0; pos < lengths[i]; pos++) {

                            BOOST_REQUIRE_EQUAL(
                               cd_msa->msa[msa_index][q_index + pos].is_aligned,
                               false);
                        }
                    } else if (q_index == kGap) {
                        s_index += lengths[i];
                        continue;
                    } else {
                        for (TSeqPos pos = 0; pos < lengths[i]; pos++) {
                            
                            // verify that aligned segments in denseg are
                            // marked as aligned in MSA
                            BOOST_REQUIRE_EQUAL(
                              cd_msa->msa[msa_index][q_index + pos].is_aligned,
                              true);

                            // verify profile data in msa
                            PSICdMsaCellData* data =
                                cd_msa->msa[msa_index][q_index + pos].data;

                            BOOST_REQUIRE(data);

                            // verify that number of independent observations
                            // is correct;
			    // we expec a small difference due to converting
			    // real numbers to integers
                            BOOST_REQUIRE(abs((Int4)(data->iobsr * kScale)
					      - (Int4)obsr[s_index + pos]) < 2);

                            // verify that residue frequencies are correct
                            for (int j=0;j < kAlphabetSize;j++) {

                                // residue frequencies in MSA may have sligtly
                                // different values than in the database,
                                // so we are only checking if frequncy 
                                // is/is not equal to zero
                                BOOST_REQUIRE_EQUAL(
                                      (int)(data->wfreqs[j] * kScale)== 0,
                                      (int)freqs[(s_index + pos)
                                                  * kAlphabetSize + j] == 0);

                            }
                        }
                    }
                    k = q_index + lengths[i];

                }

            }            

        } catch (const exception& e) {  
            cerr << e.what() << endl; 
            BOOST_REQUIRE(false);
        } catch (...) {  
            cerr << "Unknown exception" << endl; 
            BOOST_REQUIRE(false);
        }
}


// Verify that an overlapping duplicate CD is removed from alignment
BOOST_AUTO_TEST_CASE(TestDuplicateCdHits)
{
    // test overlaping duplicate hit
    CRef<CCddInputData> pssm_input = CPssmCddInputTest::CreatePssmInput(
                                CPssmCddInputTest::eDuplicateOverlappingHit);

    // verify that initially the pssm input object has 2 hits
    int pre_num_hits = CPssmCddInputTest::GetNumHits(*pssm_input);
    BOOST_REQUIRE_EQUAL(pre_num_hits, 2);

    // invoke removing duplicate hits
    CPssmCddInputTest::RemoveMultipleCdHits(*pssm_input);
    
    // verify that one hit was removed
    int post_num_hits = CPssmCddInputTest::GetNumHits(*pssm_input);
    BOOST_REQUIRE_EQUAL(post_num_hits, 1);


    // test non-overlaping duplicate hit
    pssm_input = CPssmCddInputTest::CreatePssmInput(
                        CPssmCddInputTest::eDuplicateNonOverlappingHit);

    // verify that initially the pssm inputy object has 2 hits
    pre_num_hits = CPssmCddInputTest::GetNumHits(*pssm_input);
    BOOST_REQUIRE_EQUAL(pre_num_hits, 2);

    // invoke removing duplicate hits
    CPssmCddInputTest::RemoveMultipleCdHits(*pssm_input);
    
    // verify that no hit was removed
    post_num_hits = CPssmCddInputTest::GetNumHits(*pssm_input);
    BOOST_REQUIRE_EQUAL(post_num_hits, 2);
}



//---------------------------------------------------


// Tests creating PSSM from CD alignment using CCddInputData as strategy
// Mostly verify resulting PSSM

BOOST_AUTO_TEST_CASE(TestFullPssmEngineRunWithDiagnosticsRequest) {

        const string seqalign("data/cdd-129295.asn");
        const string rpsdb("data/deltatest");

        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(seqalign, eSerial_AsnText));

        CRef<CSeq_align_set> sas(new CSeq_align_set());
        *in >> *sas;

        CRef<CSeq_id> qid(new CSeq_id("gi|129295"));

        auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(*qid));
        SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein, q->scope));

        CPSIBlastOptions opts;
        PSIBlastOptionsNew(&opts);

        PSIDiagnosticsRequest request;
        memset((void*) &request, 0, sizeof(request));
        request.information_content = true;
        request.weighted_residue_frequencies = true;
        request.frequency_ratios = true;
        request.independent_observations = true;

        const string kTitle("Test defline");

        CRef<IPssmInputCdd> pssm_strategy(new CCddInputData(
                                   seq.data.get() + 1,
                                   seq.length - 2,  
                                   sas, 
                                   *opts, 
                                   rpsdb,
                                   "BLOSUM80",
                                   11,
                                   1,
                                   &request,
                                   kTitle));

        CRef<CPssmEngine> pssm_engine(new CPssmEngine(pssm_strategy));
        CRef<CPssmWithParameters> pssm = pssm_engine->Run();

        // verify query length
        CRef<CBioseq> bioseq = pssm_strategy->GetQueryForPssm();
        BOOST_REQUIRE_EQUAL(bioseq->GetLength(), seq.length-2);

        // TO DO: verify query sequence

        string query_descr;
        if (bioseq->IsSetDescr()) {
          const CBioseq::TDescr::Tdata& data = bioseq->GetDescr().Get();
          ITERATE(CBioseq::TDescr::Tdata, iter, data) {
             if((*iter)->IsTitle()) {
                 query_descr += (*iter)->GetTitle();
             }
          }
        }
        BOOST_REQUIRE_EQUAL(query_descr, kTitle);
    

        const size_t kNumElements = 
            pssm_strategy->GetQueryLength() * BLASTAA_SIZE;


        // verify that weighted residue frequencies came back
        const CPssmIntermediateData::TWeightedResFreqsPerPos& wres_freqs =
            pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos();
        BOOST_REQUIRE_EQUAL(kNumElements, wres_freqs.size());

        // verify that frequency ratios came back
        const CPssmIntermediateData::TFreqRatios& freq_ratios = 
            pssm->GetPssm().GetIntermediateData().GetFreqRatios();
        BOOST_REQUIRE_EQUAL(kNumElements, freq_ratios.size());

        // verify that numbers of independent observations came back
        const CPssmIntermediateData::TNumIndeptObsr& obsr =
            pssm->GetPssm().GetIntermediateData().GetNumIndeptObsr();
        BOOST_REQUIRE_EQUAL(seq.length-2, obsr.size());

        // verify that pssm scores came back
        const CPssmFinalData::TScores& scores =
            pssm->GetPssm().GetFinalData().GetScores();
        BOOST_REQUIRE_EQUAL(kNumElements, scores.size());


        // TO DO: What if an unsupported diagnostic is requested?

        // currenlty unsupported diagnostics: 
        // residue_frequencies
        // gapless_columns_weights
        // sigma
        // interval_sizes
        // num_matching_seqs
}


// Verify that PSSM scores coresponding to gaps in a subject are similar to
// scores in the standard scoring matrix (BLOSUM62)
BOOST_AUTO_TEST_CASE(TestInternalGapsInSubject) {

    // create a fake Seq-align-set with valid query and subject
    CRef<CSeq_id> query_id(new CSeq_id("gi|129295"));

    // subject domain must be present in test database
    CRef<CSeq_id> subject_id(new CSeq_id("gnl|CDD|29117")); 

    string rpsdb = "data/deltatest";
    const string kMatrix = "BLOSUM62";

    // location and length of the gap
    const int kGapStart = 2;
    const int kGapLen = 160;

    CRef<CSeq_align> seq_align(new CSeq_align());
    seq_align->SetDim(2);

    // create a fake alignment with internal gap in the subject: 
    // Query:  1 QQQ...QQQQQ
    // Sbjct:  1 S--...---SS
    CDense_seg& denseg = seq_align->SetSegs().SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(3);
    CDense_seg::TIds& ids = denseg.SetIds();
    ids.push_back(query_id);
    ids.push_back(subject_id);
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();
    starts.push_back(1);
    starts.push_back(1);
    starts.push_back(kGapStart);
    starts.push_back(-1);
    starts.push_back(kGapStart + kGapLen + 1);
    starts.push_back(kGapStart + kGapLen + 1);
    lens.push_back(1);
    lens.push_back(kGapLen);
    lens.push_back(2);
    
    // make sure that denseg is valid
    denseg.Validate(true);

    seq_align->SetNamedScore(CSeq_align::eScore_EValue, 0.001);

    CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());
    seq_align_set->Set().push_back(seq_align);

    auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(*query_id));
    CRef<CScope> scope = q->scope;

    // create PSSM engine strategy
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);

    // retrieve the query sequence, but skip the sentinel bytes
    SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein, q->scope));

    CRef<IPssmInputCdd> pssm_input(new CCddInputData(seq.data.get() + 1,
                                                     seq.length - 2,
                                                     seq_align_set, *opts,
                                                     rpsdb, kMatrix));

    // compute PSSM
    CPssmEngine pssm_engine(pssm_input);
    CRef<CPssmWithParameters> pssm = pssm_engine.Run();
                                                     
    auto_ptr< CNcbiMatrix<int> > pssm_scores(
               CScorematPssmConverter::GetScores(*pssm));

    const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
    stringstream ss;
    BOOST_REQUIRE_EQUAL((size_t)pssm->GetPssm().GetNumColumns(),
                        (size_t)pssm_scores->GetCols());
    BOOST_REQUIRE_EQUAL((size_t)pssm->GetPssm().GetNumRows(),
                        (size_t)pssm_scores->GetRows());

    BOOST_REQUIRE(kGapStart + kGapLen < pssm->GetPssm().GetNumColumns());

    // Residues U, *, O, and J are not scored in the standard matrices
    const int kResiduesUOJstar = 24;

    // Verify that columns correspoding to gaps have similar PSSM scores as
    // in BLOSUM62 columns corresponing to query residues.
    // The scores may differ by 1.
    for (int i=kGapStart; i < kGapStart + kGapLen; i++) {
        for (int j = 0; j < kResiduesUOJstar; j++) {
            
            // gaps get value of BLAST_SCORE_MIN
            if (j == kGapResidue) {
                ss.str("");
                ss << "Position " << i << " residue " 
                   << TestUtil::GetResidue(j) << " differ on PSSM";
                BOOST_REQUIRE_MESSAGE(BLAST_SCORE_MIN == (*pssm_scores)(j, i),
                                      ss.str());

            } else {
                // get score from standard scoring matrix
                int bl_score = 
                    (int)NCBISM_GetScore(score_matrix,
                                         pssm_input->GetQuery()[i], j);

                ss.str("");
                ss << "Position " << i << " residue " 
                   << TestUtil::GetResidue(j) << " differ on PSSM: "
                   << "expected=" << NStr::IntToString(bl_score) 
                   << " actual=" << NStr::IntToString((*pssm_scores)(j, i));

                BOOST_REQUIRE_MESSAGE (bl_score - (*pssm_scores)(j, i) <= 1
                                       && bl_score - (*pssm_scores)(j, i) >= -1,
                                       ss.str());
            }
        }
    }
}

// Verify that PSSM can be computed for PSSM engine strategy object with
// no CDD hits. PSSM scores are in that case similar to standard scoring matrix
BOOST_AUTO_TEST_CASE(TestNoDomainHits)
{
    string rpsdb = "data/deltatest";
    const string kMatrix = "BLOSUM62";

    CRef<CSeq_id> query_id(new CSeq_id("gi|129295"));

    // Create empty Seq-align-set
    CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());

    auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(*query_id));
    CRef<CScope> scope = q->scope;

    // create PSSM engine strategy
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);

    // retrieve the query sequence, but skip the sentinel bytes
    SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein, q->scope));

    CRef<IPssmInputCdd> pssm_input(new CCddInputData(seq.data.get() + 1,
                                                     seq.length - 2,
                                                     seq_align_set, *opts,
                                                     rpsdb, kMatrix));

    // compute PSSM
    CPssmEngine pssm_engine(pssm_input);
    CRef<CPssmWithParameters> pssm = pssm_engine.Run();

    auto_ptr< CNcbiMatrix<int> > pssm_scores(
               CScorematPssmConverter::GetScores(*pssm));

    // Get BLOSUM62 scoring matrix
    const SNCBIPackedScoreMatrix* score_matrix = &NCBISM_Blosum62;
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA[(int)'-'];
    stringstream ss;
    BOOST_REQUIRE_EQUAL((size_t)pssm->GetPssm().GetNumColumns(),
                        (size_t)pssm_scores->GetCols());
    BOOST_REQUIRE_EQUAL((size_t)pssm->GetPssm().GetNumRows(),
                        (size_t)pssm_scores->GetRows());

    // Residues U, *, O, and J are not scored in the standard matrices
    const int kResiduesUOJstar = 24;

    // Verify that PSSM has scores as
    // in BLOSUM62 columns corresponing to query residues.
    // The scores may differ by 1.
    for (int i=0; i < pssm->GetPssm().GetNumColumns(); i++) {
        for (int j = 0; j < kResiduesUOJstar; j++) {

            // Exceptional residues get value of BLAST_SCORE_MIN
            if (j == kGapResidue) {
                ss.str("");
                ss << "Position " << i << " residue " 
                   << TestUtil::GetResidue(j) << " differ on PSSM";
                BOOST_REQUIRE_MESSAGE(BLAST_SCORE_MIN == (*pssm_scores)(j, i),
                                      ss.str());

            } else {
                int bl_score = 
                    (int)NCBISM_GetScore(score_matrix,
                                         pssm_input->GetQuery()[i], j);

                ss.str("");
                ss << "Position " << i << " residue " 
                   << TestUtil::GetResidue(j) << " differ on PSSM: "
                   << "expected=" << NStr::IntToString(bl_score) 
                   << " actual=" << NStr::IntToString((*pssm_scores)(j, i));

                BOOST_REQUIRE_MESSAGE (bl_score - (*pssm_scores)(j, i) <= 1
                                       && bl_score - (*pssm_scores)(j, i) >= -1,
                                       ss.str());

            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

//-----------------------------------------------------------------
// Implementation for utility classes

// Implementation for functions declared above

CRef<CSeq_align> CPssmCddInputTest::x_CreateAlignment(CRef<CSeq_id> query_id,
                                                  CRef<CSeq_id> subject_id,
                                                  int qfrom, int sfrom, int len)
{
    CRef<CSeq_align> seq_align(new CSeq_align());
    seq_align->SetDim(2);

    CDense_seg& denseg = seq_align->SetSegs().SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(1);
    CDense_seg::TIds& ids = denseg.SetIds();
    ids.push_back(query_id);
    ids.push_back(subject_id);
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();
    starts.push_back(qfrom);
    starts.push_back(sfrom);
    lens.push_back(len);
        
    // make sure that denseg is valid
    denseg.Validate(true);

    seq_align->SetNamedScore(CSeq_align::eScore_EValue, 0.001);

    return seq_align;        
}


CRef<CSeq_align_set> CPssmCddInputTest::x_CreateDuplicateOverlappingHit(
                                                   CRef<CSeq_id> query_id)
{
    CRef<CSeq_id> subject_id(new CSeq_id(kSubjectId));

    CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());
    seq_align_set->Set().push_back(x_CreateAlignment(query_id, subject_id,
                                                     0, 0, 20));

    seq_align_set->Set().push_back(x_CreateAlignment(query_id, subject_id,
                                                     0, 0, 20));
    return seq_align_set;
}


CRef<CSeq_align_set> CPssmCddInputTest::x_CreateDuplicateNonOverlappingHit(
                                                   CRef<CSeq_id> query_id)
{
    CRef<CSeq_id> subject_id(new CSeq_id(kSubjectId));

    CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());
    seq_align_set->Set().push_back(x_CreateAlignment(query_id, subject_id,
                                                     0, 0, 20));

    seq_align_set->Set().push_back(x_CreateAlignment(query_id, subject_id,
                                                     20, 10, 50));
    return seq_align_set;
}


CRef<CCddInputData> CPssmCddInputTest::CreatePssmInput(EType type)
{
    CRef<CSeq_id> query_id(new CSeq_id(kQueryId));
    const string rpsdb = "data/deltatest";
    const string kMatrix = "BLOSUM62";

    CRef<CSeq_align_set> seq_align_set;

    switch (type) {

    case eDuplicateOverlappingHit:
        seq_align_set = x_CreateDuplicateOverlappingHit(query_id);            
        break;

    case eDuplicateNonOverlappingHit:
        seq_align_set = x_CreateDuplicateNonOverlappingHit(query_id);
        break;
    }

    auto_ptr<SSeqLoc> q(CTestObjMgr::Instance().CreateSSeqLoc(*query_id));
    CRef<CScope> scope = q->scope;

    // create PSSM engine strategy
    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);

    // retrieve the query sequence, but skip the sentinel bytes
    SBlastSequence seq(GetSequence(*q->seqloc, eBlastEncodingProtein, q->scope));

    CRef<CCddInputData> pssm_input(new CCddInputData(seq.data.get() + 1,
                                                     seq.length - 2,
                                                     seq_align_set, *opts,
                                                     rpsdb, kMatrix));

    pssm_input->x_ProcessAlignments(-1.0, 10.0);

    return pssm_input;
}


size_t CPssmCddInputTest::GetMsaSize(const CCddInputData& input)
{
    return input.m_MsaData.size();
}

int CPssmCddInputTest::GetAlphabetSize(void)
{
    return CCddInputData::kAlphabetSize;
}

int CPssmCddInputTest::GetProfilesDataScale(void)
{
    return CCddInputData::kRpsScaleFactor;
}

int CPssmCddInputTest::GetSubjectMsaIndex(const CCddInputData& input,
                                             const CSeq_id& subject)
{
    int retval = -1;
        
    for (int i=0;i < (int)input.m_Hits.size();i++) {
        if (input.m_Hits[i]->m_SubjectId->Match(subject)) {
            retval = i;
            break;
        }
    }

    return retval;
}

size_t CPssmCddInputTest::GetNumHits(const CCddInputData& input)
{
    return input.m_Hits.size();
}

void CPssmCddInputTest::RemoveMultipleCdHits(CCddInputData& input)
{
    input.x_RemoveMultipleCdHits();
}


CPssmInputWithNoCDs::CPssmInputWithNoCDs(void)
{
    // query with the same residue: A
    m_Query.resize(kQueryLength, 1);

    m_Dimensions.query_length = kQueryLength;
    m_Dimensions.num_seqs = 0;

    m_CdMsa.msa = NULL;
    m_CdMsa.dimensions = &m_Dimensions;
    m_CdMsa.query = &m_Query[0];

    PSIBlastOptionsNew(&m_Options);
}


CPssmInputWithSingleCD::CPssmInputWithSingleCD(void)
{
    // domain residue frequencies
    int freqs[] = {0, 2, 2, 1, 5, 0, 0, 0, 0, 0, 
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0};
        
    // domain independent observations per column times 10
    int obsr[] = {21, 34, 56, 21, 21, 21};

    m_Freqs.resize(28);
    double sum = 0.0;
    for (int i=0;i < 28;i++) {
        m_Freqs[i] = (double)freqs[i] / 10.0;
        sum += m_Freqs[i];
    }
    BOOST_REQUIRE_CLOSE(sum, 1.0, 1e-5);


    PSICdMsaCellData data;
    data.wfreqs = NULL;
    data.iobsr = 0.0;
    m_MsaData.resize(kQueryLength, data);

    // this is to prevent reallocation in child classes
    m_Msa.reserve(10);
    m_Msa.push_back(new PSICdMsaCell[kQueryLength]);
    m_CdMsa.msa = &m_Msa[0];

    for (int i=0;i < kQueryLength;i++) {
        m_Msa[0][i].is_aligned = true;
        m_Msa[0][i].data = &m_MsaData[i];
        m_Msa[0][i].data->wfreqs = &m_Freqs[0];
        m_Msa[0][i].data->iobsr = (double)obsr[i] / 10.0;
    }

    m_Dimensions.num_seqs = 1;
}

CPssmInputWithSingleCD::~CPssmInputWithSingleCD()
{
    ITERATE (vector<PSICdMsaCell*>, it, m_Msa) {
        delete [] *it;
    }
}

CPssmInputWithTwoCDs::CPssmInputWithTwoCDs(void)
{
    m_Msa.resize(2);
    m_Msa[1] = new PSICdMsaCell[kQueryLength];

    int obsr[] = {22, 41, 76, 21, 200, 21};

    for (int i=0;i < kQueryLength;i++) {
        m_Msa[1][i].is_aligned = true;
        m_Msa[1][i].data = &m_MsaData[i];
        m_Msa[1][i].data->wfreqs = &m_Freqs[0];
        m_Msa[1][i].data->iobsr = (double)obsr[i] / 10.0;
    }

    m_Dimensions.num_seqs = 2;
    m_CdMsa.msa = &m_Msa[0];
}



// Test computing frequency ratios and PSSM scores
void s_TestCreatePssmFromFreqs(const PSICdMsa* cd_msa, CBlastScoreBlk& sbp,
                               const PSIBlastOptions* opts,
                               AutoPtr<_PSISequenceWeights>& seq_weights)
{

    // compute and verify frequency ratios

    AutoPtr<_PSIInternalPssmData> internal_pssm(
                 _PSIInternalPssmDataNew(cd_msa->dimensions->query_length,
                                         (Uint4)sbp->alphabet_size));
                                         
    // pre conditions
    BOOST_REQUIRE_EQUAL(internal_pssm->ncols, cd_msa->dimensions->query_length);
    BOOST_REQUIRE_EQUAL(internal_pssm->nrows, (unsigned int)sbp->alphabet_size);
    

    // verify that the function returns success
    BOOST_REQUIRE(_PSIComputeFreqRatiosFromCDs(cd_msa, seq_weights.get(),
                                               sbp.Get(), opts->pseudo_count,
                                               internal_pssm.get()) == 0);

    // post conditions
    if (cd_msa->dimensions->num_seqs > 0) {
        for (int i=0;i < (int)cd_msa->dimensions->query_length;i++) {
            for (int j=0;j < (int)sbp->alphabet_size;j++) {

                // verify that frequency ratios for residues with non-zero
                // background frequencies are non-zero
                BOOST_REQUIRE_EQUAL(internal_pssm->freq_ratios[i][j] < 1e-5,
                                    seq_weights->std_prob[j] < 1e-5);
            }
        }
    }


    // compute and verify PSSM scores

    // verify that the function returns success
    BOOST_REQUIRE(_PSIConvertFreqRatiosToPSSM(
                     internal_pssm.get(), cd_msa->query,
                     sbp.Get(), seq_weights->std_prob) == 0);

    const Uint4 kXResidue = AMINOACID_TO_NCBISTDAA[(int)'X'];
    const Uint4 kStarResidue = AMINOACID_TO_NCBISTDAA[(int)'*'];

    // post conditions
    if (cd_msa->dimensions->num_seqs > 0) {
        for (int i=0;i < (int)cd_msa->dimensions->query_length;i++) {
            for (Uint4 j=0;j < (Uint4)sbp->alphabet_size;j++) {

                // skip 'X' and '*' residues
                if (j == kXResidue || j == kStarResidue) {
                    continue;
                }

                // get the true frequency ratio
                double q_over_p_estimate = internal_pssm->freq_ratios[i][j]
                    / seq_weights->std_prob[j];

                // verify that non-zero frequency ration result in scores larger
                // than the minimum score
                BOOST_REQUIRE_EQUAL(q_over_p_estimate > 1e-5,
                                    internal_pssm->scaled_pssm[i][j]
                                    > BLAST_SCORE_MIN);

                // verify that frequency ratios > 1 result in scores > 0
                BOOST_REQUIRE_EQUAL(q_over_p_estimate > 1.0,
                                    internal_pssm->scaled_pssm[i][j] >= 0);

                // verify that frequency ratios < 1 result in scores < 0
                BOOST_REQUIRE_EQUAL(q_over_p_estimate < 1.0
                                    && q_over_p_estimate > 1e-5,
                                    internal_pssm->scaled_pssm[i][j] <= 0
                                    && internal_pssm->scaled_pssm[i][j]
                                    > BLAST_SCORE_MIN);
            }
        }
    }
}
