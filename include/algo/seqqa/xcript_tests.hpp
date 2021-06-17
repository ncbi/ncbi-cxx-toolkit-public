#ifndef ALGO_SEQQA___XSCRIPT_TESTS__HPP
#define ALGO_SEQQA___XSCRIPT_TESTS__HPP

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
 * Authors:  Mike DiCuccio, Josh Cherry
 *
 * File Description:
 *
 */

#include <algo/seqqa/seqtest.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XALGOSEQQA_EXPORT CTestTranscript : public CSeqTest
{
public:
    bool CanTest(const CSerialObject& obj, const CSeqTestContext* ctx) const;
};

#define DECLARE_TRANSCRIPT_TEST(name) \
class NCBI_XALGOSEQQA_EXPORT CTestTranscript_##name : public CTestTranscript \
{ \
public: \
    CRef<objects::CSeq_test_result_set> \
        RunTest(const CSerialObject& obj, \
                const CSeqTestContext* ctx); \
}


///
/// Count the number of annotated coding regions
///
DECLARE_TRANSCRIPT_TEST(CountCdregions);

///
/// Check coding regions for "partial", "pseudo", and "except" flags
///
DECLARE_TRANSCRIPT_TEST(CdsFlags);

///
/// Check to see if the sequence contains a CDS feature that has an
/// in-frame upstream start codon
///
DECLARE_TRANSCRIPT_TEST(InframeUpstreamStart);

///
/// Check to see if the sequence contains a CDS feature that has an
/// in-frame upstream stop codon
///
DECLARE_TRANSCRIPT_TEST(InframeUpstreamStop);

///
/// Measure the coding propensity of the CDS feature annotated on a
/// CDS feature
///
DECLARE_TRANSCRIPT_TEST(CodingPropensity);

///
/// Measure the length of an mRNA sequence
///
DECLARE_TRANSCRIPT_TEST(TranscriptLength);

///
/// Measure the length of an CDS feature on a transcript sequence
///
DECLARE_TRANSCRIPT_TEST(TranscriptCdsLength);

///
/// Check to see if the sequence contains a CDS feature that has a
/// start codon
///
DECLARE_TRANSCRIPT_TEST(CdsStartCodon);

///
/// Check to see if the sequence contains a CDS feature that has a
/// stop codon
///
DECLARE_TRANSCRIPT_TEST(CdsStopCodon);

///
/// Check CDS features for premature stop codons
///
DECLARE_TRANSCRIPT_TEST(PrematureStopCodon);

///
/// Compare the annotated protein product of a CDS in the database 
/// to actual translation of that CDS
///
DECLARE_TRANSCRIPT_TEST(CompareProtProdToTrans);

///
/// Compare the annotated protein product of a CDS in the database 
/// to actual translation of that CDS
///
DECLARE_TRANSCRIPT_TEST(Utrs);

///
/// Count the number of As at the end of the sequence
///
DECLARE_TRANSCRIPT_TEST(PolyA);

///
/// Categorize ORFs in this transcript sequence
///
DECLARE_TRANSCRIPT_TEST(Orfs);

///
/// Count code-breaks on CDS
///
DECLARE_TRANSCRIPT_TEST(Code_break);

///
/// See how far the annotated CDS can be extended 5'
/// to an ATG without hitting a stop.
///
DECLARE_TRANSCRIPT_TEST(OrfExtension);

///
/// Count ambiguous residues in transcript and CDS
///
DECLARE_TRANSCRIPT_TEST(CountAmbiguities);

END_NCBI_SCOPE

#endif  // ALGO_SEQQA___XSCRIPT_TESTS__HPP
