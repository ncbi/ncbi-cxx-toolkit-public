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
* File Description:
*   Unit test module for the blast options handle class
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <algo/blast/core/blast_def.h>

#include "test_objmgr.hpp"

#ifdef NCBI_OS_IRIX
#include <stdlib.h>
#else
#include <cstdlib>
#endif

// template function to invoke mutator/accessor (setter/getter) member 
// functions on classes derived from the class BC and verifies the assignment
// using BOOST_REQUIRE_EQUAL
template <class BC, class T>
void VerifyMutatorAccessor(BC& obj, 
                           void (BC::*mutator)(T), 
                           T (BC::*accessor)(void) const, 
                           T& expected_value)
{
#   define CALL_MEMBER_FUNCTION(obj, membFnPtr) ((obj).*(membFnPtr))

    CALL_MEMBER_FUNCTION(obj, mutator)(expected_value);
    T actual_value = CALL_MEMBER_FUNCTION(obj, accessor)();
    BOOST_REQUIRE_EQUAL(expected_value, actual_value);
}

using namespace std;
using namespace ncbi;
using namespace ncbi::blast;

struct UniversalOptiosHandleFixture {
    UniversalOptiosHandleFixture() {
        // Use a randomly chosen program to ensure all derived classes support
        // these methods.  Addition and subtraction of one ensures that the
        // results is not zero (eBlastNotSet).
        EProgram p = (EProgram) (1 + rand() % ((int)eBlastProgramMax - 1));
        m_OptsHandle = CBlastOptionsFactory::Create(p);
    }
    ~UniversalOptiosHandleFixture() { delete m_OptsHandle;}
    
    CBlastOptionsHandle* m_OptsHandle;
};

// Test the "universal" BLAST optins (apply to all programs)
// TLM - CBlastOptionsHandleTest

BOOST_FIXTURE_TEST_CASE(Set_Get_MaskAtHash_Universal, UniversalOptiosHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CBlastOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetMaskAtHash,
             &CBlastOptionsHandle::GetMaskAtHash, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapXDropoff_Universal, UniversalOptiosHandleFixture) {
        double value = 10.5;

        VerifyMutatorAccessor<CBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetGapXDropoff,
             &CBlastOptionsHandle::GetGapXDropoff, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapTrigger_Universal, UniversalOptiosHandleFixture) {
        double value = 10.5;

        VerifyMutatorAccessor<CBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetGapTrigger,
             &CBlastOptionsHandle::GetGapTrigger, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_HitlistSize_Universal, UniversalOptiosHandleFixture) {
        int value = 100;

        VerifyMutatorAccessor<CBlastOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetHitlistSize,
             &CBlastOptionsHandle::GetHitlistSize, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_MaxNumHspPerSequence_Universal, UniversalOptiosHandleFixture) {
        int value = 100;

        VerifyMutatorAccessor<CBlastOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetMaxNumHspPerSequence,
             &CBlastOptionsHandle::GetMaxNumHspPerSequence, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_EvalueThreshold_Universal, UniversalOptiosHandleFixture) {
        double value = -10.5;

        VerifyMutatorAccessor<CBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetEvalueThreshold,
             &CBlastOptionsHandle::GetEvalueThreshold, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_CutoffScore_Universal, UniversalOptiosHandleFixture) {
        int value = -10;

        VerifyMutatorAccessor<CBlastOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetCutoffScore,
             &CBlastOptionsHandle::GetCutoffScore, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_PercentIdentity_Universal, UniversalOptiosHandleFixture) {
        double value = 1.5;

        VerifyMutatorAccessor<CBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetPercentIdentity,
             &CBlastOptionsHandle::GetPercentIdentity, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GappedMode_Universal, UniversalOptiosHandleFixture) {
        bool value = false;

        VerifyMutatorAccessor<CBlastOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetGappedMode,
             &CBlastOptionsHandle::GetGappedMode, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_Culling_Universal, UniversalOptiosHandleFixture) {
        int value = 20;

        VerifyMutatorAccessor<CBlastOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastOptionsHandle::SetCullingLimit,
             &CBlastOptionsHandle::GetCullingLimit, 
             value);
}

// Test creation of BlastOptionsHandle.
// TLM - CBlastOptionsCreateTaskTest

BOOST_AUTO_TEST_CASE(BlastnTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("blastn"); 
       CBlastNucleotideOptionsHandle* opts =
             dynamic_cast<CBlastNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE_EQUAL(2, opts->GetMatchReward());
       delete handle;
}

BOOST_AUTO_TEST_CASE(BlastnShortTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("blastn-short"); 
       CBlastNucleotideOptionsHandle* opts =
             dynamic_cast<CBlastNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE_EQUAL(1, opts->GetMatchReward());
       BOOST_REQUIRE_EQUAL(7, opts->GetWordSize());
       delete handle;
}

BOOST_AUTO_TEST_CASE(MegablastTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("megablast"); 
       CBlastNucleotideOptionsHandle* opts =
             dynamic_cast<CBlastNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE_EQUAL(1, opts->GetMatchReward());
       delete handle;
}

BOOST_AUTO_TEST_CASE(DCMegablastTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("dc-megablast"); 
       CDiscNucleotideOptionsHandle* opts =
             dynamic_cast<CDiscNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE_EQUAL(2, opts->GetMatchReward());
       BOOST_REQUIRE_EQUAL(18, (int) opts->GetTemplateLength());
       delete handle;
}

BOOST_AUTO_TEST_CASE(CaseSensitiveTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("MeGaBlaSt"); 
       CBlastNucleotideOptionsHandle* opts =
             dynamic_cast<CBlastNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       delete handle;
}

BOOST_AUTO_TEST_CASE(BadNameTest) {
       CBlastOptionsHandle* handle = NULL;
       BOOST_CHECK_THROW(handle = CBlastOptionsFactory::CreateTask("mega"),
                         CBlastException); 
       CBlastNucleotideOptionsHandle* opts =
             dynamic_cast<CBlastNucleotideOptionsHandle*> (handle);
       BOOST_REQUIRE(opts == NULL);
       delete handle;
}

BOOST_AUTO_TEST_CASE(BlastpTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("blastp"); 
       CBlastAdvancedProteinOptionsHandle* opts =
             dynamic_cast<CBlastAdvancedProteinOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE(!strcmp("BLOSUM62", opts->GetMatrixName()));
       BOOST_REQUIRE_EQUAL(3, opts->GetWordSize());
       delete handle;
}

BOOST_AUTO_TEST_CASE(BlastpShortTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("blastp-short"); 
       CBlastAdvancedProteinOptionsHandle* opts =
             dynamic_cast<CBlastAdvancedProteinOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE(!strcmp("PAM30", opts->GetMatrixName()));
       BOOST_REQUIRE_EQUAL(2, opts->GetWordSize());
       delete handle;
}

BOOST_AUTO_TEST_CASE(BlastxTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("blastx"); 
       CBlastxOptionsHandle* opts =
             dynamic_cast<CBlastxOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE(!strcmp("BLOSUM62", opts->GetMatrixName()));
       BOOST_REQUIRE_EQUAL(3, opts->GetWordSize());
       delete handle;
}

BOOST_AUTO_TEST_CASE(TblastnTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("tblastn"); 
       CTBlastnOptionsHandle* opts =
             dynamic_cast<CTBlastnOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE(!strcmp("BLOSUM62", opts->GetMatrixName()));
       BOOST_REQUIRE_EQUAL(3, opts->GetWordSize());
       delete handle;
}

BOOST_AUTO_TEST_CASE(TblastxTest) {
       CBlastOptionsHandle* handle = CBlastOptionsFactory::CreateTask("tblastx"); 
       CTBlastxOptionsHandle* opts =
             dynamic_cast<CTBlastxOptionsHandle*> (handle);
       BOOST_REQUIRE(opts != NULL);
       BOOST_REQUIRE(!strcmp("BLOSUM62", opts->GetMatrixName()));
       BOOST_REQUIRE_EQUAL(3, opts->GetWordSize());
       delete handle;
}


struct ProteinOptiosHandleFixture {
    ProteinOptiosHandleFixture() {
        m_OptsHandle = new CBlastProteinOptionsHandle();
    }
    ~ProteinOptiosHandleFixture() { delete m_OptsHandle;}
    
    CBlastProteinOptionsHandle* m_OptsHandle;
};


// Protein options. 
// TLM - CBlastProtOptionsHandleTest

BOOST_FIXTURE_TEST_CASE(Set_Get_WordThreshold_Protein, ProteinOptiosHandleFixture) {
        double value = 15;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetWordThreshold,
             &CBlastProteinOptionsHandle::GetWordThreshold, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_WordSize_Protein, ProteinOptiosHandleFixture) {
        int value = 5;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetWordSize,
             &CBlastProteinOptionsHandle::GetWordSize, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_WindowSize_Protein, ProteinOptiosHandleFixture) {
        int value = 50;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetWindowSize,
             &CBlastProteinOptionsHandle::GetWindowSize, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_XDropoff_Protein, ProteinOptiosHandleFixture) {
        double value = 26.2;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetXDropoff,
             &CBlastProteinOptionsHandle::GetXDropoff, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapXDropoffFinal_Protein, ProteinOptiosHandleFixture) {
        double value = 26.2;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetGapXDropoffFinal,
             &CBlastProteinOptionsHandle::GetGapXDropoffFinal, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DbLength_Protein, ProteinOptiosHandleFixture) {
        Int8 value = 1000000;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, Int8>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetDbLength,
             &CBlastProteinOptionsHandle::GetDbLength, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DbSeqNum_Protein, ProteinOptiosHandleFixture) {
        unsigned int value = 0x1<<16;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, unsigned int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetDbSeqNum,
             &CBlastProteinOptionsHandle::GetDbSeqNum, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_EffectiveSearchSpace_Protein, ProteinOptiosHandleFixture) {
        Int8 value = 1000000;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, Int8>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetEffectiveSearchSpace,
             &CBlastProteinOptionsHandle::GetEffectiveSearchSpace, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFiltering_Protein, ProteinOptiosHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetSegFiltering,
             &CBlastProteinOptionsHandle::GetSegFiltering, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFilteringWindow_Protein, ProteinOptiosHandleFixture) {
        int value = 26;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetSegFilteringWindow,
             &CBlastProteinOptionsHandle::GetSegFilteringWindow, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_SegWindowWithSegOptionsUnallocated_Protein, ProteinOptiosHandleFixture) {

        m_OptsHandle->SetSegFiltering(false); // turn off SEG filtering.
        // the following call should turn it on again.
        int value = m_OptsHandle->GetSegFilteringWindow();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFilteringLocut_Protein, ProteinOptiosHandleFixture) {
        double value = 1.7;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetSegFilteringLocut,
             &CBlastProteinOptionsHandle::GetSegFilteringLocut, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_SegLocutWithSegOptionsUnallocated_Protein, ProteinOptiosHandleFixture) {

        m_OptsHandle->SetSegFiltering(false); // turn off SEG filtering.
        // the following call should turn it on again.
        double value = m_OptsHandle->GetSegFilteringLocut();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFilteringHicut_Protein, ProteinOptiosHandleFixture) {
        double value = 3.7;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetSegFilteringHicut,
             &CBlastProteinOptionsHandle::GetSegFilteringHicut, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_SegHicutWithSegOptionsUnallocated_Protein, ProteinOptiosHandleFixture) {

        m_OptsHandle->SetSegFiltering(false); // turn off SEG filtering.
        // the following call should turn it on again.
        double value = m_OptsHandle->GetSegFilteringHicut();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_MatrixName_Protein, ProteinOptiosHandleFixture) {
        const char* value = "dummy matrix";

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, const char*>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetMatrixName,
             &CBlastProteinOptionsHandle::GetMatrixName, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapOpeningCost_Protein, ProteinOptiosHandleFixture) {
        int value = 150;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetGapOpeningCost,
             &CBlastProteinOptionsHandle::GetGapOpeningCost, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapExtensionCost_Protein, ProteinOptiosHandleFixture) {
        int value = 150;

        VerifyMutatorAccessor<CBlastProteinOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastProteinOptionsHandle::SetGapExtensionCost,
             &CBlastProteinOptionsHandle::GetGapExtensionCost, 
             value);
}

struct PSIBlastOptiosHandleFixture {
    PSIBlastOptiosHandleFixture() {
        m_OptsHandle = new CPSIBlastOptionsHandle();
    }
    ~PSIBlastOptiosHandleFixture() { delete m_OptsHandle;}
    
    CPSIBlastOptionsHandle* m_OptsHandle;
};


// PSI-BLAST options
// TLM - CPSIBlastOptionsHandleTest

BOOST_FIXTURE_TEST_CASE(Set_Get_WordThreshold_PSIBlast, PSIBlastOptiosHandleFixture) {
        double value = 15;

        VerifyMutatorAccessor<CPSIBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CPSIBlastOptionsHandle::SetWordThreshold,
             &CPSIBlastOptionsHandle::GetWordThreshold, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_InclusionThreshold_PSIBlast, PSIBlastOptiosHandleFixture) {
        double value = 0.05;

        VerifyMutatorAccessor<CPSIBlastOptionsHandle, double>
            (*m_OptsHandle, 
             &CPSIBlastOptionsHandle::SetInclusionThreshold,
             &CPSIBlastOptionsHandle::GetInclusionThreshold, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFiltering_PSIBlast, PSIBlastOptiosHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CPSIBlastOptionsHandle, bool>
            (*m_OptsHandle, 
             &CPSIBlastOptionsHandle::SetSegFiltering,
             &CPSIBlastOptionsHandle::GetSegFiltering, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SegFilteringWindow_PSIBlast, PSIBlastOptiosHandleFixture) {
        int value = 26;

        VerifyMutatorAccessor<CPSIBlastOptionsHandle, int>
            (*m_OptsHandle, 
             &CPSIBlastOptionsHandle::SetSegFilteringWindow,
             &CPSIBlastOptionsHandle::GetSegFilteringWindow, 
             value);
}

struct AdvancedProteinOptionsHandleFixture {
    AdvancedProteinOptionsHandleFixture() {
        m_OptsHandle = new CBlastAdvancedProteinOptionsHandle();
    }
    ~AdvancedProteinOptionsHandleFixture() { delete m_OptsHandle;}
    
    CBlastAdvancedProteinOptionsHandle* m_OptsHandle;
};

// Advanced Protein options
// TLM - CBlastAdvancedProtOptionsHandleTest
BOOST_FIXTURE_TEST_CASE(Set_Get_CompositionBasedStats_AdvancedProtein, AdvancedProteinOptionsHandleFixture) {
        ECompoAdjustModes value = eNoCompositionBasedStats;

        VerifyMutatorAccessor<CBlastAdvancedProteinOptionsHandle,
                              ECompoAdjustModes>
            (*m_OptsHandle, 
             &CBlastAdvancedProteinOptionsHandle::SetCompositionBasedStats,
             &CBlastAdvancedProteinOptionsHandle::GetCompositionBasedStats, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_SmithWatermanMode_AdvancedProtein, AdvancedProteinOptionsHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CBlastAdvancedProteinOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastAdvancedProteinOptionsHandle::SetSmithWatermanMode,
             &CBlastAdvancedProteinOptionsHandle::GetSmithWatermanMode, 
             value);
}

struct BlastNuclOptionsHandleFixture {
    BlastNuclOptionsHandleFixture() {
        m_OptsHandle = new CBlastNucleotideOptionsHandle();
    }
    ~BlastNuclOptionsHandleFixture() { delete m_OptsHandle;}
    
    CBlastNucleotideOptionsHandle* m_OptsHandle;
};

// Nucleotide blast
// TLM - CBlastNuclOptionsHandleTest

BOOST_FIXTURE_TEST_CASE(Set_Get_LookupTableType_BlastNucl, BlastNuclOptionsHandleFixture) {
        ELookupTableType value = eNaLookupTable;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, ELookupTableType>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetLookupTableType,
             &CBlastNucleotideOptionsHandle::GetLookupTableType, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_WordSize_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 23;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetWordSize,
             &CBlastNucleotideOptionsHandle::GetWordSize, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_StrandOption_BlastNucl, BlastNuclOptionsHandleFixture) {
        objects::ENa_strand value = objects::eNa_strand_minus;
        m_OptsHandle->SetStrandOption(value);
        objects::ENa_strand actual_value = m_OptsHandle->GetStrandOption();
        BOOST_REQUIRE_EQUAL((int)value, (int)actual_value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_WindowSize_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 50;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetWindowSize,
             &CBlastNucleotideOptionsHandle::GetWindowSize, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_XDropoff_BlastNucl, BlastNuclOptionsHandleFixture) {
        double value = 40;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetXDropoff,
             &CBlastNucleotideOptionsHandle::GetXDropoff, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapXDropoffFinal_BlastNucl, BlastNuclOptionsHandleFixture) {
        double value = 100;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, double>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapXDropoffFinal,
             &CBlastNucleotideOptionsHandle::GetGapXDropoffFinal, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapExtnAlgorithm_BlastNucl, BlastNuclOptionsHandleFixture) {
        EBlastPrelimGapExt value = eDynProgScoreOnly;
        const int kGapOpen = 7;
        const int kGapExtend = 3;

        m_OptsHandle->SetGapOpeningCost(kGapOpen);
        m_OptsHandle->SetGapExtensionCost(kGapExtend);

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, EBlastPrelimGapExt>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapExtnAlgorithm,
             &CBlastNucleotideOptionsHandle::GetGapExtnAlgorithm, 
             value);

        BOOST_REQUIRE_EQUAL(kGapOpen, m_OptsHandle->GetGapOpeningCost());
        BOOST_REQUIRE_EQUAL(kGapExtend, m_OptsHandle->GetGapExtensionCost());

        value = eGreedyScoreOnly;
        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, EBlastPrelimGapExt>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapExtnAlgorithm,
             &CBlastNucleotideOptionsHandle::GetGapExtnAlgorithm, 
             value);

        BOOST_REQUIRE_EQUAL(kGapOpen, m_OptsHandle->GetGapOpeningCost());
        BOOST_REQUIRE_EQUAL(kGapExtend, m_OptsHandle->GetGapExtensionCost());

}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapTracebackAlgorithm_BlastNucl, BlastNuclOptionsHandleFixture) {
        EBlastTbackExt value = eDynProgTbck;
        const int kGapOpen = 7;
        const int kGapExtend = 3;

        m_OptsHandle->SetGapOpeningCost(kGapOpen);
        m_OptsHandle->SetGapExtensionCost(kGapExtend);

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, EBlastTbackExt>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapTracebackAlgorithm,
             &CBlastNucleotideOptionsHandle::GetGapTracebackAlgorithm, 
             value);

        BOOST_REQUIRE_EQUAL(kGapOpen, m_OptsHandle->GetGapOpeningCost());
        BOOST_REQUIRE_EQUAL(kGapExtend, m_OptsHandle->GetGapExtensionCost());

}

BOOST_FIXTURE_TEST_CASE(Set_Get_MatchReward_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 2;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetMatchReward,
             &CBlastNucleotideOptionsHandle::GetMatchReward, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_MismatchPenalty_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = -3;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetMismatchPenalty,
             &CBlastNucleotideOptionsHandle::GetMismatchPenalty, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_MatrixName_BlastNucl, BlastNuclOptionsHandleFixture) {
        const char* value = "MYNAMATRIX";

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, const char*>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetMatrixName,
             &CBlastNucleotideOptionsHandle::GetMatrixName, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapOpeningCost_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 4;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapOpeningCost,
             &CBlastNucleotideOptionsHandle::GetGapOpeningCost, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_GapExtensionCost_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 1;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetGapExtensionCost,
             &CBlastNucleotideOptionsHandle::GetGapExtensionCost, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_EffectiveSearchSpace_BlastNucl, BlastNuclOptionsHandleFixture) {
        Int8 value = 20000000;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, Int8>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetEffectiveSearchSpace,
             &CBlastNucleotideOptionsHandle::GetEffectiveSearchSpace, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DustFiltering_BlastNucl, BlastNuclOptionsHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetDustFiltering,
             &CBlastNucleotideOptionsHandle::GetDustFiltering, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DustFilteringLevel_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 20;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetDustFilteringLevel,
             &CBlastNucleotideOptionsHandle::GetDustFilteringLevel, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_DustLevelWithDustOptionsUnallocated_BlastNucl, BlastNuclOptionsHandleFixture) {

        m_OptsHandle->SetDustFiltering(false); // turn off dust filtering.
        // the following call should turn it on again.
        int value = m_OptsHandle->GetDustFilteringLevel();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DustFilteringWindow_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 21;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetDustFilteringWindow,
             &CBlastNucleotideOptionsHandle::GetDustFilteringWindow, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_DustWindowWithDustOptionsUnallocated_BlastNucl, BlastNuclOptionsHandleFixture) {

        m_OptsHandle->SetDustFiltering(false); // turn off dust filtering.
        // the following call should turn it on again.
        int value = m_OptsHandle->GetDustFilteringWindow();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_DustFilteringLinker_BlastNucl, BlastNuclOptionsHandleFixture) {
        int value = 22;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetDustFilteringLinker,
             &CBlastNucleotideOptionsHandle::GetDustFilteringLinker, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Get_DustLinkerWithDustOptionsUnallocated_BlastNucl, BlastNuclOptionsHandleFixture) {

        m_OptsHandle->SetDustFiltering(false); // turn off dust filtering.
        // the following call should turn it on again.
        int value = m_OptsHandle->GetDustFilteringLinker();
        BOOST_REQUIRE(value < 0);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_RepeatFiltering_BlastNucl, BlastNuclOptionsHandleFixture) {
        bool value = true;

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, bool>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetRepeatFiltering,
             &CBlastNucleotideOptionsHandle::GetRepeatFiltering, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_RepeatFilteringDB_BlastNucl, BlastNuclOptionsHandleFixture) {
        const char* db = "my_repeat_db";

        VerifyMutatorAccessor<CBlastNucleotideOptionsHandle, const char*>
            (*m_OptsHandle, 
             &CBlastNucleotideOptionsHandle::SetRepeatFilteringDB,
             &CBlastNucleotideOptionsHandle::GetRepeatFilteringDB, 
             db);
}

struct DiscNucleotideOptionsHandleFixture {
    DiscNucleotideOptionsHandleFixture() {
        m_OptsHandle = new CDiscNucleotideOptionsHandle();
    }
    ~DiscNucleotideOptionsHandleFixture() { delete m_OptsHandle;}
    
    CDiscNucleotideOptionsHandle* m_OptsHandle;
};


// Discontiguous nucleotide blast
// TLM - CDiscNuclOptionsHandleTest
BOOST_FIXTURE_TEST_CASE(Set_Get_TemplateLength_DiscNucleotide, DiscNucleotideOptionsHandleFixture) {
        unsigned char value = 18;

        VerifyMutatorAccessor<CDiscNucleotideOptionsHandle, unsigned char>
            (*m_OptsHandle, 
             &CDiscNucleotideOptionsHandle::SetTemplateLength,
             &CDiscNucleotideOptionsHandle::GetTemplateLength, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_TemplateType_DiscNucleotide, DiscNucleotideOptionsHandleFixture) {
        unsigned char value = 1;

        VerifyMutatorAccessor<CDiscNucleotideOptionsHandle, unsigned char>
            (*m_OptsHandle, 
             &CDiscNucleotideOptionsHandle::SetTemplateType,
             &CDiscNucleotideOptionsHandle::GetTemplateType, 
             value);
}

BOOST_FIXTURE_TEST_CASE(Set_Get_WordSize_DiscNucleotide, DiscNucleotideOptionsHandleFixture) {
        int value = 12;

        VerifyMutatorAccessor<CDiscNucleotideOptionsHandle, int>
            (*m_OptsHandle, 
             &CDiscNucleotideOptionsHandle::SetWordSize,
             &CDiscNucleotideOptionsHandle::GetWordSize, 
             value);
        value = 16;
        try {
            m_OptsHandle->SetWordSize(value);
        } catch (const CBlastException& exptn) {
            BOOST_REQUIRE(!strcmp("Word size must be 11 or 12 only", exptn.GetMsg().c_str()));
        }
}

BOOST_FIXTURE_TEST_CASE(Set_Get_QueryCoverageHspPercentage, BlastNuclOptionsHandleFixture) {

        int value = m_OptsHandle->GetQueryCovHspPerc();
        //Test default
        BOOST_REQUIRE(value == 0);
        m_OptsHandle->SetQueryCovHspPerc(52);
        value = m_OptsHandle->GetQueryCovHspPerc();
        BOOST_REQUIRE(value == 52);
}




//BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: optionshandle-cppunit.cpp,v $
* Revision 1.32  2008/07/18 14:16:43  camacho
* Minor fix to previous commit
*
* Revision 1.31  2008/07/18 14:05:21  camacho
* Irix fixes
*
* Revision 1.30  2007/10/23 16:00:57  madden
* Changes for removal of [SG]etUngappedExtension
*
* Revision 1.29  2007/07/25 12:41:39  madden
* Accomodates changes to blastn type defaults
*
* Revision 1.28  2007/07/10 13:52:40  madden
* tests of CBlastOptionsFactory::CreateTask (CBlastOptionsCreateTaskTest)
*
* Revision 1.27  2007/04/05 13:00:20  madden
* 2nd arg to SetFilterString
*
* Revision 1.26  2007/03/07 19:20:41  papadopo
* make lookup table threshold a double
*
* Revision 1.25  2007/02/14 20:18:01  papadopo
* remove SetFullByteScan and discontig. megablast with stride 4
*
* Revision 1.24  2007/02/08 17:13:49  papadopo
* change enum value
*
* Revision 1.23  2006/12/19 16:38:23  madden
* Fix if filtering option is NULL
*
* Revision 1.22  2006/12/13 13:52:35  madden
* Add CPSIBlastOptionsHandleTest
*
* Revision 1.21  2006/11/28 13:29:30  madden
* Ensure that eBlastNotSet is never chosen as a program
*
* Revision 1.20  2006/11/21 17:47:36  papadopo
* use enum for lookup table type
*
* Revision 1.19  2006/06/12 17:23:41  madden
* Remove [GS]etMatrixPath
*
* Revision 1.18  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.17  2006/01/23 19:57:52  camacho
* Allow new varieties of composition based statistics
*
* Revision 1.16  2005/12/22 14:17:00  papadopo
* remove variable wordsize test
*
* Revision 1.15  2005/08/01 12:55:28  madden
* Check that SetGapTracebackAlgorithm and SetGapExtnAlgorithm do not change gap costs
*
* Revision 1.14  2005/05/24 19:16:22  camacho
* Register advanced options handle tests with a unique name
*
* Revision 1.13  2005/05/24 18:48:25  madden
* Add CBlastAdvancedProtOptionsHandleTest
*
* Revision 1.12  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.11  2005/03/02 22:39:10  camacho
* Remove deprecated methods
*
* Revision 1.10  2005/02/24 13:48:58  madden
* Add tests of getters and setters for structured filtering options
*
* Revision 1.9  2005/01/10 14:57:40  madden
* Add Set_Get_FullByteScan for discontiguous megablast
*
* Revision 1.8  2005/01/10 14:04:30  madden
* Removed calls to methods that no longer exist
*
* Revision 1.7  2004/12/28 13:37:48  madden
* Use an int rather than a short for word size
*
* Revision 1.6  2004/08/30 16:54:29  dondosha
* Added unit tests for nucleotide and discontiguous options handles setters and getters
*
* Revision 1.5  2004/07/06 19:40:11  camacho
* Remove extra qualification of assertion_traits
*
* Revision 1.4  2004/03/10 15:54:06  madden
* Changes for rps options handle
*
* Revision 1.3  2004/02/20 23:20:37  camacho
* Remove undefs.h
*
* Revision 1.2  2003/12/12 16:16:33  camacho
* Minor
*
* Revision 1.1  2003/11/26 18:47:13  camacho
* Initial revision. Intended as example of CppUnit framework use
*
*
* ===========================================================================
*/
