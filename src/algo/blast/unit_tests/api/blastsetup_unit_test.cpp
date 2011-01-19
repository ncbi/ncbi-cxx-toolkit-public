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
*   Unit test module for blast_setup_cxx.cpp from internal/c++/algo/blast/api
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <util/util_misc.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/metareg.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/readers/fasta.hpp>

#include <algo/blast/api/blast_types.hpp>
#include "dust_filter.hpp"
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <blast_objmgr_priv.hpp>

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/gencode_singleton.h>

#include <algo/blast/api/effsearchspace_calc.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

#include <string>
#include <vector>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

class CBlastSetupTestFixture
{
public:
    CTestObjMgr* m_Om;

    CBlastSetupTestFixture() {
        m_Om = &CTestObjMgr::Instance();
    }

    ~CBlastSetupTestFixture() {
        m_Om = NULL;
    }

    // N.B.: when calling SetupSubjects, it doesn't matter what strand is
    // specified for the nucleotide subject, both strands will be filtered.
    static void s_TestSingleSubjectNuclMask(BLAST_SequenceBlk const * seqblk) {
        BOOST_REQUIRE(seqblk->lcase_mask == 0);
        BOOST_REQUIRE(seqblk->lcase_mask_allocated == FALSE);
        BOOST_REQUIRE(seqblk->seq_ranges != NULL);
        BOOST_REQUIRE(seqblk->num_seq_ranges >= 1);
    }

    /// Auxiliary function to validate that the query sequence data hasn't been
    /// modified (filtering can change the actual sequence data...)
    static void s_ValidateProtein130912(CRef<IQueryFactory> query_factory,
                                const CBlastOptions& opts,
                                string prefix) {
        const Uint4 kGi_130912_Length = 253;
        const Uint1 kGi_130912_[kGi_130912_Length] = { 
        12,  1, 13, 11,  7,  3, 20, 12, 11, 19, 11,  6, 19,  1, 18, 
        20, 17,  4, 11,  7, 11,  3, 10, 10, 16, 14, 10, 14,  7,  7, 
        20, 13, 18,  7,  7, 17, 16, 22, 14,  7, 15,  7, 17, 14,  7, 
            7, 13, 16, 22, 14, 14, 15,  7,  7,  7,  7, 20,  7, 15, 14, 
            8,  7,  7,  7, 20,  7, 15, 14,  8,  7,  7,  7, 20,  7, 15, 
        14,  8,  7,  7,  7, 20,  7, 15, 14,  8,  7,  7,  7, 20,  7, 
        15,  7,  7,  7, 18,  8, 17, 15, 20, 13, 10, 14, 17, 10, 14, 
        10, 18, 13, 12, 10,  8, 12,  1,  7,  1,  1,  1,  1,  7,  1, 
        19, 19,  7,  7, 11,  7,  7, 22, 12, 11,  7, 17,  1, 12, 17, 
        16, 14,  9,  9,  8,  6,  7, 17,  4, 22,  5,  4, 16, 22, 22, 
        16,  5, 13, 12,  8, 16, 22, 14, 13, 15, 19, 22, 22, 16, 14, 
        12,  4,  5, 22, 17, 13, 15, 13, 13,  6, 19,  8,  4,  3, 19, 
        13,  9, 18,  9, 10, 15,  8, 18, 19, 18, 18, 18, 18, 10,  7, 
            5, 13,  6, 18,  5, 18,  4, 19, 10, 12, 12,  5, 16, 19, 19, 
            5, 15, 12,  3,  9, 18, 15, 22,  5, 16,  5, 17, 15,  1, 22, 
        22, 15, 16,  7, 17, 17, 12, 19, 11,  6, 17, 17, 14, 14, 19, 
            9, 11, 11,  9, 17,  6, 11,  9,  6, 11,  9, 19,  7};

        CRef<ILocalQueryData> query_data =
            query_factory->MakeLocalQueryData(&opts);
        BLAST_SequenceBlk* seq_blk = query_data->GetSequenceBlk();
        BOOST_REQUIRE_EQUAL((int)kGi_130912_Length, seq_blk->length);

        for (int i = 0; i < seq_blk->length; i++) {
            ostringstream os;
            os << prefix << ": position " << i << endl;
            BOOST_REQUIRE_MESSAGE((int)seq_blk->sequence[i] == (int)kGi_130912_[i], 
                                    os.str());
        }
    }

    static BlastScoringOptions * s_GetScoringOpts(const CBlastOptions& opts)
    {
        return opts.GetScoringOpts();
    }

    static BlastEffectiveLengthsOptions * s_GetEffLenOpts(const CBlastOptions& opts)
    {
        return opts.GetEffLenOpts();
    }
};

BOOST_FIXTURE_TEST_SUITE(BlastSetup, CBlastSetupTestFixture)

/********* ncbi::blast::GetNumberOfContexts() **************/
BOOST_AUTO_TEST_CASE(NumberOfContextsBlastp) {
    BOOST_REQUIRE_EQUAL(1, (int)GetNumberOfContexts(eBlastTypeBlastp));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsBlastn) {
    BOOST_REQUIRE_EQUAL(NUM_STRANDS, 
                            (int)GetNumberOfContexts(eBlastTypeBlastn));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsBlastx) {
    BOOST_REQUIRE_EQUAL(NUM_FRAMES, 
                            (int)GetNumberOfContexts(eBlastTypeBlastx));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsTblastx) {
    BOOST_REQUIRE_EQUAL(NUM_FRAMES, 
                            (int)GetNumberOfContexts(eBlastTypeTblastx));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsTblastn) {
    BOOST_REQUIRE_EQUAL(1, 
                            (int)GetNumberOfContexts(eBlastTypeTblastn));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsRPSBlast) {
    BOOST_REQUIRE_EQUAL(1, 
                            (int)GetNumberOfContexts(eBlastTypeRpsBlast));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsRPSTblastn) {
    BOOST_REQUIRE_EQUAL(NUM_FRAMES, 
                            (int)GetNumberOfContexts(eBlastTypeRpsTblastn));
}

BOOST_AUTO_TEST_CASE(NumberOfContextsThrow) {
    // this should certainly throw an exception
    BOOST_REQUIRE_THROW(GetNumberOfContexts(eBlastTypeUndefined),
                        CBlastException);
}

/********* Functions in algo/blast/core/blast_program.h **************/
BOOST_AUTO_TEST_CASE(QueryIsTranslated) {
    const vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();
    ITERATE(vector<EBlastProgramType>, program, programs) {
        bool reference = (*program == eBlastTypeBlastx ||
                            *program == eBlastTypeTblastx ||
                            *program == eBlastTypeRpsTblastn);
        bool test = Blast_QueryIsTranslated(*program);
        string prog(Blast_ProgramNameFromType(*program));
        BOOST_REQUIRE_MESSAGE(reference == test, "Failed on " + prog);
    }
}

BOOST_AUTO_TEST_CASE(QueryIsPlainNucleotide) {
    const vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();
    ITERATE(vector<EBlastProgramType>, program, programs) {
        bool reference = (*program == eBlastTypeBlastn);
        bool test = Blast_QueryIsNucleotide(*program) &&
            !Blast_QueryIsTranslated(*program) &&
            !Blast_ProgramIsPhiBlast(*program);
        string prog(Blast_ProgramNameFromType(*program));
        BOOST_REQUIRE_MESSAGE(reference == test, "Failed on " + prog);
    }
}

BOOST_AUTO_TEST_CASE(SubjectIsProtein) {
    const vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();
    ITERATE(vector<EBlastProgramType>, program, programs) {
        bool reference = (*program == eBlastTypeBlastp ||
                            *program == eBlastTypeBlastx ||
                            *program == eBlastTypePsiBlast ||
                            Blast_ProgramIsRpsBlast(*program) ||
                            *program == eBlastTypePhiBlastp);
        bool test = Blast_SubjectIsProtein(*program);
        string prog(Blast_ProgramNameFromType(*program));
        BOOST_REQUIRE_MESSAGE(reference == test, "Failed on " + prog);
    }
}

BOOST_AUTO_TEST_CASE(SubjectIsNucleotide) {
    const vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();
    ITERATE(vector<EBlastProgramType>, program, programs) {
        bool reference = (*program == eBlastTypeBlastn ||
                            *program == eBlastTypeTblastn ||
                            *program == eBlastTypeTblastx ||
                            *program == eBlastTypePsiTblastn ||
                            *program == eBlastTypePhiBlastn);
        bool test = Blast_SubjectIsNucleotide(*program);
        string prog(Blast_ProgramNameFromType(*program));
        BOOST_REQUIRE_MESSAGE(reference == test, "Failed on " + prog);
    }
}

BOOST_AUTO_TEST_CASE(SubjectIsPlainNucleotide) {
    const vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();
    ITERATE(vector<EBlastProgramType>, program, programs) {
        bool reference = (*program == eBlastTypeBlastn);
        bool test = Blast_SubjectIsNucleotide(*program) &&
            !Blast_SubjectIsTranslated(*program) &&
            !Blast_ProgramIsPhiBlast(*program);
        string prog(Blast_ProgramNameFromType(*program));
        BOOST_REQUIRE_MESSAGE(reference == test, "Failed on " + prog);
    }
}

/********* CORE function BlastNumber2Program **********************/
BOOST_AUTO_TEST_CASE(BlastNumber2Program_CORE) {
    vector<EBlastProgramType> programs(TestUtil::GetAllBlastProgramTypes());
    programs.push_back(static_cast<EBlastProgramType>(eBlastTypeUndefined));

    vector<string> strings;
    strings.reserve(programs.size());
    strings.push_back("blastp");
    strings.push_back("blastn");
    strings.push_back("blastx");
    strings.push_back("tblastn");
    strings.push_back("tblastx");
    strings.push_back("psiblast");
    strings.push_back("psitblastn");
    strings.push_back("rpsblast");
    strings.push_back("rpstblastn");
    strings.push_back("phiblastp");
    strings.push_back("phiblastn");
    strings.push_back("unknown");

    BOOST_REQUIRE_EQUAL(programs.size(), strings.size());

    for (size_t i = 0; i < programs.size(); i++) {
        BOOST_REQUIRE_EQUAL(Blast_ProgramNameFromType(programs[i]),
                                strings[i]);
    }
}

/********* ncbi::blast::ProgramNameToEnum() **************/
BOOST_AUTO_TEST_CASE(String2Enum_Blastp) {
    BOOST_REQUIRE_EQUAL(eBlastp, ProgramNameToEnum("BlAsTp"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Blastn) {
    BOOST_REQUIRE_EQUAL(eBlastn, ProgramNameToEnum("BlAsTN"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Blastx) {
    BOOST_REQUIRE_EQUAL(eBlastx, ProgramNameToEnum("BlAsTx"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Tblastx) {
    BOOST_REQUIRE_EQUAL(eTblastx, ProgramNameToEnum("TBlAsTx"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Tblastn) {
    BOOST_REQUIRE_EQUAL(eTblastn, ProgramNameToEnum("TBlAsTn"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Megablast) {
    BOOST_REQUIRE_EQUAL(eMegablast, ProgramNameToEnum("megablast"));
}

BOOST_AUTO_TEST_CASE(String2Enum_DiscoMegablast) {
    BOOST_REQUIRE_EQUAL(eDiscMegablast, ProgramNameToEnum("dc-megablast"));
}

BOOST_AUTO_TEST_CASE(String2Enum_PSIBlast) {
    BOOST_REQUIRE_EQUAL(ePSIBlast, ProgramNameToEnum("psiblast"));
}

BOOST_AUTO_TEST_CASE(String2Enum_RPSBlast) {
    BOOST_REQUIRE_EQUAL(eRPSBlast, ProgramNameToEnum("RPSBlast"));
}

BOOST_AUTO_TEST_CASE(String2Enum_RPSTblastn) {
    BOOST_REQUIRE_EQUAL(eRPSTblastn, ProgramNameToEnum("rpstblastn"));
}

BOOST_AUTO_TEST_CASE(String2Enum_Throw1) {
    BOOST_REQUIRE_THROW(ProgramNameToEnum("junk"), CBlastException);
}

BOOST_AUTO_TEST_CASE(String2Enum_PSITblastn) {
    BOOST_REQUIRE_EQUAL(ePSITblastn, ProgramNameToEnum("psitblastn"));
}

/********* ncbi::blast::FindMatrixPath() **************/
BOOST_AUTO_TEST_CASE(FindMatrixPathSuccess) {
    TAutoCharPtr input = strdup("blosum62");
    char* matrix_path = BlastFindMatrixPath(input.get(), true);
    BOOST_REQUIRE((matrix_path != NULL) && (strlen(matrix_path) > 0));
    sfree(matrix_path);
}

//Test empty return value
BOOST_AUTO_TEST_CASE(FindMatrixPathNoNcbiRcFile) {
    TAutoCharPtr input = strdup("blosum30");
    string	ignoreFile("BLOSUM30");
    g_IgnoreDataFile(ignoreFile, true);
    char* matrix_path = BlastFindMatrixPath(input.get(), true);
    g_IgnoreDataFile(ignoreFile, false);
    BOOST_REQUIRE(matrix_path == NULL);
    sfree(matrix_path);
}

BOOST_AUTO_TEST_CASE(FindMatrixPathTestBLASTMAT) {
        CAutoEnvironmentVariable a("BLASTMAT", "data");
        TAutoCharPtr input = strdup("BLOSUMTEST");
        string	ignoreFile(input.get());
        g_IgnoreDataFile(ignoreFile, true);
        char* matrix_path = BlastFindMatrixPath(input.get(), true);
        g_IgnoreDataFile(ignoreFile, false);
        BOOST_REQUIRE((matrix_path != NULL) && (strlen(matrix_path) > 0));
}

BOOST_AUTO_TEST_CASE(FindMatrixPathFailure) {
    TAutoCharPtr input = strdup("a non-existent matrix");
    char* matrix_path = BlastFindMatrixPath(input.get(), true);
    BOOST_REQUIRE(matrix_path == NULL);
}

BOOST_AUTO_TEST_CASE(FindMatrixPathWithNull) {
    char* matrix_path = BlastFindMatrixPath(NULL, false);
    BOOST_REQUIRE(matrix_path == NULL);
}

BOOST_AUTO_TEST_CASE(Context2Frame_ProteinQuery) {
    const size_t kNumPrograms = 4;
    EBlastProgramType program[kNumPrograms] = { 
        eBlastTypeBlastp, 
        eBlastTypeTblastn, 
        eBlastTypeRpsBlast,
        eBlastTypePsiBlast
    };
    const char* program_strings[kNumPrograms] = { 
        "blastp", 
        "tblastn",
        "rpsblast",
        "psiblast"
    };
    string error_prefix("Conversion from frame to context failed for ");

    for (size_t i = 0; i < kNumPrograms; i++) {
        Int1 frame;

        frame = BLAST_ContextToFrame(program[i], 4);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 57);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);

        frame = BLAST_ContextToFrame(program[i], 8);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 1);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);

        frame = BLAST_ContextToFrame(program[i], 26);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 71);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);

        frame = BLAST_ContextToFrame(program[i], 33);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 12);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)0,
                                error_prefix + program_strings[i]);
    }
}

BOOST_AUTO_TEST_CASE(Context2Frame_NucleotideQuery) {
    // odd returns -1, even returns 1
    Int1 frame;
    string error("Conversion from frame to context failed for blastn");

    frame = BLAST_ContextToFrame(eBlastTypeBlastn, 0);
    BOOST_REQUIRE_MESSAGE((int)frame == (int)1, error);
    frame = BLAST_ContextToFrame(eBlastTypeBlastn, 1);
    BOOST_REQUIRE_MESSAGE((int)frame == (int)-1, error);
    frame = BLAST_ContextToFrame(eBlastTypeBlastn, 33);
    BOOST_REQUIRE_MESSAGE((int)frame == (int)-1, error);
    frame = BLAST_ContextToFrame(eBlastTypeBlastn, 42);
    BOOST_REQUIRE_MESSAGE((int)frame == (int)1, error);
}

BOOST_AUTO_TEST_CASE(Context2Frame_TranslatedQuery) {
    const size_t kNumPrograms = 3;
    EBlastProgramType program[kNumPrograms] = { 
        eBlastTypeBlastx,
        eBlastTypeTblastx,
        eBlastTypeRpsTblastn
    };
    const char* program_strings[kNumPrograms] = { 
        "blastx", 
        "tblastx",
        "rpstblastn"
    };
    string error_prefix("Conversion from frame to context failed for ");

    for (size_t i = 0; i < kNumPrograms; i++) {
        Int1 frame;

        frame = BLAST_ContextToFrame(program[i], 63);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)-1,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 28);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)-2,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 5);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)-3,
                                error_prefix + program_strings[i]);

        frame = BLAST_ContextToFrame(program[i], 6);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)1,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 37);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)2,
                                error_prefix + program_strings[i]);
        frame = BLAST_ContextToFrame(program[i], 56);
        BOOST_REQUIRE_MESSAGE((int)frame == (int)3,
                                error_prefix + program_strings[i]);
    }

}

BOOST_AUTO_TEST_CASE(Context2Frame_Error) {
    Int1 frame;
    
    frame = BLAST_ContextToFrame(eBlastTypeUndefined, 100);
    BOOST_REQUIRE_EQUAL((int)127, (int)frame);
    frame = BLAST_ContextToFrame(eBlastTypeUndefined, 35);
    BOOST_REQUIRE_EQUAL((int)127, (int)frame);
}

BOOST_AUTO_TEST_CASE(SetupSubjectsProt) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sseqloc(CTestObjMgr::Instance().CreateSSeqLoc(id));

    TSeqLocVector seqloc_v;
    seqloc_v.push_back(*sseqloc);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastp));
    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle->GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    unsigned int seqlen = sequence::GetLength(id, sseqloc->scope);
    BOOST_REQUIRE(maxlen != 0);
    BOOST_REQUIRE(seqlen != 0);
    BOOST_REQUIRE(seqblk_v.size() == (size_t)1);
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);
    // Make sure there is no mask
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BOOST_REQUIRE((*itr)->lcase_mask == NULL);
    }

    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsProtRange) {
    CSeq_id id("gi|129295");
    pair<TSeqPos, TSeqPos> range(50, 157);
    auto_ptr<SSeqLoc> sseqloc(
        CTestObjMgr::Instance().CreateSSeqLoc(id, range));

    TSeqLocVector seqloc_v;
    seqloc_v.push_back(*sseqloc);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastp));
    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle->GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    unsigned int seqlen = range.second - range.first + 1;
    BOOST_REQUIRE(maxlen != 0);
    BOOST_REQUIRE(seqlen != 0);
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);
    BOOST_REQUIRE(seqblk_v.size() == (size_t)1);
    // Make sure there is no mask
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BOOST_REQUIRE((*itr)->lcase_mask == NULL);
    }

    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsNuclPlusStrandFiltered) {
    CSeq_id id(CSeq_id::e_Gi, 1945388);
    const TSeqRange kRange(0, 480);
    const ENa_strand kStrand(eNa_strand_plus);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, kRange, kStrand));
    TSeqLocVector seqloc_v(1, *sl);
    CBlastNucleotideOptionsHandle opts_handle;
    Blast_FindDustFilterLoc(seqloc_v, &opts_handle);
    BOOST_REQUIRE(seqloc_v.begin()->mask.NotEmpty());

    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle.GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    BOOST_REQUIRE(seqblk_v.size() == 1);
    unsigned int seqlen = kRange.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);

    s_TestSingleSubjectNuclMask(*seqblk_v.begin());

    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsNuclMinusStrandFiltered) {
    CSeq_id id(CSeq_id::e_Gi, 1945388);
    const TSeqRange kRange(0, 480);
    const ENa_strand kStrand(eNa_strand_minus);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, kRange, kStrand));
    TSeqLocVector seqloc_v(1, *sl);
    CBlastNucleotideOptionsHandle opts_handle;
    Blast_FindDustFilterLoc(seqloc_v, &opts_handle);
    BOOST_REQUIRE(seqloc_v.begin()->mask.NotEmpty());

    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle.GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    BOOST_REQUIRE(seqblk_v.size() == 1);
    unsigned int seqlen = kRange.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);

    s_TestSingleSubjectNuclMask(*seqblk_v.begin());

    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsNuclBothStrandFiltered) {
    CSeq_id id(CSeq_id::e_Gi, 1945388);
    const TSeqRange kRange(0, 480);
    const ENa_strand kStrand(eNa_strand_both);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, kRange, kStrand));
    TSeqLocVector seqloc_v(1, *sl);
    CBlastNucleotideOptionsHandle opts_handle;
    Blast_FindDustFilterLoc(seqloc_v, &opts_handle);
    BOOST_REQUIRE(seqloc_v.begin()->mask.NotEmpty());

    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle.GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    BOOST_REQUIRE(seqblk_v.size() == 1);
    unsigned int seqlen = kRange.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);

    s_TestSingleSubjectNuclMask(*seqblk_v.begin());

    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}


BOOST_AUTO_TEST_CASE(SetupSubjectsNuclPlusStrand) {

    CSeq_id id("NT_004487.15");
    TSeqRange range(0, 480);
    const ENa_strand kStrand(eNa_strand_plus);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, range, kStrand));
    TSeqLocVector seqloc_v(1, *sl);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastn));
    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle->GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    unsigned int seqlen = range.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);
    // Make sure there is no mask
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BOOST_REQUIRE((*itr)->lcase_mask == NULL);
    }
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsNuclMinusStrand) {

    CSeq_id id("NT_004487.15");
    const TSeqRange range(0, 480);
    const ENa_strand kStrand(eNa_strand_minus);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, range, kStrand));
    TSeqLocVector seqloc_v(1, *sl);
    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastn));
    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle->GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    unsigned int seqlen = range.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);
    // Make sure there is no mask
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BOOST_REQUIRE((*itr)->lcase_mask == NULL);
    }
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(SetupSubjectsNuclBothStrand) {

    CSeq_id id("NT_004487.15");
    const TSeqRange range(0, 480);
    const ENa_strand kStrand(eNa_strand_both);
    auto_ptr<SSeqLoc> sl(m_Om->CreateSSeqLoc(id, range, kStrand));
    TSeqLocVector seqloc_v(1, *sl);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastn));
    vector<BLAST_SequenceBlk*> seqblk_v;
    unsigned int maxlen = 0;

    SetupSubjects(seqloc_v, opts_handle->GetOptions().GetProgramType(), 
                    &seqblk_v, &maxlen);

    unsigned int seqlen = range.GetLength();
    BOOST_REQUIRE_EQUAL(maxlen, seqlen);
    // Make sure there is no mask
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BOOST_REQUIRE((*itr)->lcase_mask == NULL);
    }
    ITERATE(vector<BLAST_SequenceBlk*>, itr, seqblk_v) {
        BlastSequenceBlkFree(*itr);
    }
}

BOOST_AUTO_TEST_CASE(GetSequenceProteinWithSelenocysteine) {
    CRef<CObjectManager> kObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*kObjMgr));
    CRef<CSeq_entry> seq_entry;
    const string kFile("data/selenocysteines.fsa");
    const string kSeqIdString("lcl|seq1");
    ifstream in(kFile.c_str());
    if ( !in ) {
        throw runtime_error("Failed to open " + kFile);
    }
    
    CFastaReader reader(in);
    bool read_failed = false;
    try
    {
        seq_entry = reader.ReadSet();
    }
    catch (...)
    {
        read_failed = true;
    }

    if ( read_failed || !seq_entry ) {
        throw runtime_error("Failed to read sequence from " + kFile);
    }
    scope->AddTopLevelSeqEntry(*seq_entry);
    CRef<CSeq_loc> seqloc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id(kSeqIdString));
    seqloc->SetWhole(*id);

    string warnings;
    SBlastSequence seq(GetSequence(*seqloc, eBlastEncodingProtein, scope,
                                    eNa_strand_unknown, eSentinels, 
                                    &warnings));
    BOOST_REQUIRE(!warnings.empty());

    // Check that the sequence has its selenocysteine residues replaced by
    // X's (positions 10 and 15, without counting sentinels);
    const TSeqPos kReplacedPositions[] = { 10+1, 15+1 };
    const Uint1 kXresidue = AMINOACID_TO_NCBISTDAA[(int)'X'];
    for (TSeqPos i = 0; i < DIM(kReplacedPositions); i++) {
        BOOST_REQUIRE_EQUAL((int)kXresidue, 
                                (int)seq.data.get()[kReplacedPositions[i]]);
    }

}

BOOST_AUTO_TEST_CASE(GetSequenceNCBI4NA_NoSentinels) {
    CSeq_id qid("NT_004487.16");
    pair<TSeqPos, TSeqPos> range(0, 19);
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                                eNa_strand_plus));

    TSeqPos expected_length = 20;
    SBlastSequence seq = 
        GetSequence(*sl->seqloc, eBlastEncodingNcbi4na, sl->scope,
                                eNa_strand_plus, eNoSentinels);
    BOOST_REQUIRE_EQUAL(expected_length, seq.length);
    // TODO: Check each of the characters for correctness
}

BOOST_AUTO_TEST_CASE(GetSequenceNCBI2NA) {
    CSeq_id qid("NT_004487.16");
    pair<TSeqPos, TSeqPos> range(0, 19);
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                                eNa_strand_plus));

    TSeqPos expected_length = (TSeqPos) 20/COMPRESSION_RATIO + 1;
    SBlastSequence seq(
        GetSequence(*sl->seqloc, eBlastEncodingNcbi2na, sl->scope,
                                eNa_strand_plus, eNoSentinels));
    BOOST_REQUIRE_EQUAL(expected_length, seq.length);
    // TODO: Check each of the characters for correctness
}

BOOST_AUTO_TEST_CASE(GetGapOnlySequenceNCBI2NA) {
    CSeq_id sid("gi|28487070");
    pair<TSeqPos, TSeqPos> range(63999900,64000000);
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, 
                                                eNa_strand_plus));

    SBlastSequence seq(
        GetSequence(*sl->seqloc, eBlastEncodingNcbi2na, sl->scope,
                                eNa_strand_plus, eNoSentinels));

    Uint1* sequence = seq.data.get();
    int hash_value=0;
    for (TSeqPos i = 0; i < seq.length; i++)
    {
        hash_value += *sequence;
        sequence++;
    }
    
    BOOST_REQUIRE_EQUAL(3467, hash_value);
    BOOST_REQUIRE_EQUAL((TSeqPos) 26, seq.length); // 26 is (64000000-63999900+1)/4 + 1
}

BOOST_AUTO_TEST_CASE(GetGapInSequenceNCBI2NA) {
    CSeq_id sid("NT_081921.1");
    pair<TSeqPos, TSeqPos> range(3471240, 3686557);
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, 
                                                eNa_strand_plus));

    SBlastSequence seq(
        GetSequence(*sl->seqloc, eBlastEncodingNcbi2na, sl->scope,
                                eNa_strand_plus, eNoSentinels));

    Uint1* sequence = seq.data.get();
    int hash_value=0;
    for (TSeqPos i = 0; i < seq.length; i++)
    {
        hash_value += *sequence;
        sequence++;
    }
    
    BOOST_REQUIRE_EQUAL(6942427, hash_value);
    BOOST_REQUIRE_EQUAL((TSeqPos) 53830, seq.length); 
}

BOOST_AUTO_TEST_CASE(testCalcEffLengths)
{
    const int kNumPrograms = 7;
    const EProgram kProgram[kNumPrograms] = 
        { eBlastn, eBlastp, eBlastx, eTblastn, eTblastx, eRPSBlast, 
        eRPSTblastn };
    const int kNuclGi = 555;
    const int kProtGi = 129295;
    // Use values for database length and number of sequences that 
    // approximate the real ones for nt.00 and nr as of 12/17/2004.
    const Int8 kNuclDbLength = (Int8) 39855e+5;
    const Int8 kProtDbLength = (Int8) 75867e+4;
    const Int4 kNuclNumDbSeqs = (Int4) 1140e+3;
    const Int4 kProtNumDbSeqs = (Int4) 2247e+3;
    // The correct result values for search space and length adjustment,
    // per program.
    const double kSearchSp[kNumPrograms] = 
        { 2333197e+6, 532990e+5, 423501e+5, 122988e+6, 192228e+6, 
        532990e+5, 423501e+5 };
    const int kLengthAdjustments[kNumPrograms] = 
        { 33, 122, 121, 128, 56, 122, 121 };
    // Only check precision to the number of digits in the correct values
    // for the search space above.
    const double kMaxRelativeError = 5e-6; 

    int index;



    for (index = 0; index < kNumPrograms; ++index) {
        TSeqLocVector query_v;
        CRef<CSeq_loc> loc(new CSeq_loc());
        bool query_is_prot = 
            (kProgram[index] == eBlastp || kProgram[index] == eTblastn ||
            kProgram[index] == eRPSBlast);
        if (query_is_prot)
            loc->SetWhole().SetGi(kProtGi);
        else
            loc->SetWhole().SetGi(kNuclGi);
        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        query_v.push_back(SSeqLoc(loc, scope));

        CRef<CBlastOptionsHandle> opts(
            CBlastOptionsFactory::Create(kProgram[index]));

        if (index >= 5) {
            opts->SetOptions().SetMatrixName(BLAST_DEFAULT_MATRIX);
            opts->SetOptions().SetGapOpeningCost(11);
            opts->SetOptions().SetGapExtensionCost(1);
        }
        CBlastQueryInfo query_info;
        CBLAST_SequenceBlk query_blk;
        TSearchMessages msgs;

        const CBlastOptions& kOpts = opts->GetOptions();
        EBlastProgramType prog = kOpts.GetProgramType();
        ENa_strand strand_opt = kOpts.GetStrandOption();

        SetupQueryInfo(query_v, prog, strand_opt, &query_info);
        SetupQueries(query_v, query_info, &query_blk, 
                    prog, strand_opt, msgs);

        ITERATE(TSearchMessages, m, msgs) {
            BOOST_REQUIRE(m->empty());
        }
        BlastScoreBlk* sbp;
        Blast_Message* blast_message = NULL;
        Int2 status = 
            BlastSetup_ScoreBlkInit(query_blk, query_info, 
                                    s_GetScoringOpts(opts->GetOptions()), 
                                    opts->GetOptions().GetProgramType(), 
                                    &sbp, 1.0, &blast_message,
                                    &BlastFindMatrixPath);
        blast_message = Blast_MessageFree(blast_message);
        BOOST_REQUIRE(status == 0);
        
        BlastEffectiveLengthsParameters* eff_len_params = NULL;
        BlastEffectiveLengthsParametersNew(s_GetEffLenOpts(opts->GetOptions()),
                                            0, 0, &eff_len_params);
        bool db_is_nucl = (kProgram[index] == eBlastn || 
                            kProgram[index] == eTblastn ||
                            kProgram[index] == eTblastx);
        eff_len_params->real_db_length = 
            (db_is_nucl ? kNuclDbLength : kProtDbLength);
        eff_len_params->real_num_seqs = 
            (db_is_nucl ? kNuclNumDbSeqs : kProtNumDbSeqs);

        BLAST_CalcEffLengths(opts->GetOptions().GetProgramType(), 
                            s_GetScoringOpts(opts->GetOptions()),
                            eff_len_params, sbp, query_info, NULL);
        
        double relative_error = 
            fabs((kSearchSp[index] - 
                (double) query_info->contexts[0].eff_searchsp) /
                kSearchSp[index]);
        BOOST_REQUIRE(relative_error < kMaxRelativeError);
        BOOST_REQUIRE_EQUAL(kLengthAdjustments[index], 
                            query_info->contexts[0].length_adjustment);
        

        // Compare results with those produced by the
        // CEffectiveSearchSpaceCalculator
        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_v));
        CEffectiveSearchSpaceCalculator essc(qf, opts->GetOptions(), 
                                            eff_len_params->real_num_seqs, 
                                            eff_len_params->real_db_length);

        relative_error = fabs((kSearchSp[index] -
                                (double) essc.GetEffSearchSpace()) /
                            kSearchSp[index]);
        BOOST_REQUIRE(relative_error < kMaxRelativeError);

        BlastScoreBlkFree(sbp);
        BlastEffectiveLengthsParametersFree(eff_len_params);
    }
}

BOOST_AUTO_TEST_CASE(testEffSearchSpaceCalculator_Ungapped) {
    CSeq_id id("gi|130912");
    auto_ptr<SSeqLoc> sseqloc(CTestObjMgr::Instance().CreateSSeqLoc(id));

    TSeqLocVector query_v;
    query_v.push_back(*sseqloc);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastp));
    opts_handle->SetOptions().SetGappedMode(false);

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_v));
    s_ValidateProtein130912(qf, opts_handle->GetOptions(),
                            "Before CEffectiveSearchSpaceCalculator");
    
    // Calculate the effective search space for searching the ecoli.aa
    // database
    CEffectiveSearchSpaceCalculator essc(qf, opts_handle->GetOptions(), 
                                        4289, 1358990);
    Int8 eff_searchsp = essc.GetEffSearchSpace();
    BOOST_REQUIRE(eff_searchsp > 0);

    s_ValidateProtein130912(qf, opts_handle->GetOptions(),
                            "After CEffectiveSearchSpaceCalculator");
}

BOOST_AUTO_TEST_CASE(testEffSearchSpaceCalculatorNoSideEffects) {
    CSeq_id id("gi|130912");
    auto_ptr<SSeqLoc> sseqloc(CTestObjMgr::Instance().CreateSSeqLoc(id));

    TSeqLocVector query_v;
    query_v.push_back(*sseqloc);

    auto_ptr<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastp));

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_v));
    s_ValidateProtein130912(qf, opts_handle->GetOptions(),
                            "Before CEffectiveSearchSpaceCalculator");
    
    // Calculate the effective search space for searching the ecoli.aa
    // database
    CEffectiveSearchSpaceCalculator essc(qf, opts_handle->GetOptions(), 
                                        4289, 1358990);
    Int8 eff_searchsp = essc.GetEffSearchSpace();
    BOOST_REQUIRE(eff_searchsp > 0);

    s_ValidateProtein130912(qf, opts_handle->GetOptions(),
                            "After CEffectiveSearchSpaceCalculator");
}

BOOST_AUTO_TEST_CASE(testCalcEffLengthsWithUserSearchSpace)
{
    const Int8 kSearchSp = (Int8) 1e+9;
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const int kQueryLength = 1000;

    BlastScoringOptions* score_opts = NULL;
    BlastScoringOptionsNew(kProgram, &score_opts);

    BlastScoreBlk* sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    Blast_ScoreBlkMatrixInit(kProgram, score_opts, sbp, &BlastFindMatrixPath); 
    Blast_ScoreBlkKbpIdealCalc(sbp);
    
    BlastQueryInfo* query_info = BlastQueryInfoNew(kProgram, 1);
    query_info->contexts[0].query_length = kQueryLength;
    sbp->kbp_gap = sbp->kbp_gap_std;
    sbp->kbp_gap[0] = (Blast_KarlinBlk*) malloc(sizeof(Blast_KarlinBlk));
    Blast_KarlinBlkCopy(sbp->kbp_gap[0], sbp->kbp_ideal);

    BlastEffectiveLengthsOptions* eff_len_opts = NULL;
    BlastEffectiveLengthsOptionsNew(&eff_len_opts);
    BLAST_FillEffectiveLengthsOptions(eff_len_opts, 0, 0, 
                                        (Int8 *)&kSearchSp, 1);
    BlastEffectiveLengthsParameters* eff_len_params = NULL;
    BlastEffectiveLengthsParametersNew(eff_len_opts, 0, 0, &eff_len_params);

    BLAST_CalcEffLengths(kProgram, score_opts, eff_len_params, sbp, 
                            query_info, NULL);

    BOOST_REQUIRE_EQUAL(kSearchSp, 
                            query_info->contexts[0].eff_searchsp);
    BlastEffectiveLengthsOptionsFree(eff_len_opts);
    BlastEffectiveLengthsParametersFree(eff_len_params);
    BlastScoreBlkFree(sbp);
    BlastScoringOptionsFree(score_opts);
    BlastQueryInfoFree(query_info);
}

BOOST_AUTO_TEST_CASE(testMitochondrialGeneticCode) {
    GenCodeSingletonInit();

    CSeq_id id("NC_001321");
    const EBlastProgramType kProgram = eBlastTypeBlastx;
    const ENa_strand kStrand = eNa_strand_both;

    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id,
                                                    kStrand));
    TSeqLocVector query_v;
    query_v.push_back(*sl);
    BOOST_REQUIRE_EQUAL((Uint4)1, query_v[0].genetic_code_id);

    CBlastQueryInfo query_info;
    SetupQueryInfo(query_v, kProgram, kStrand, &query_info);

    TSearchMessages msgs;
    CBLAST_SequenceBlk query_blk;
    SetupQueries(query_v, query_info, &query_blk, 
                    kProgram, kStrand, msgs);

    BOOST_REQUIRE_EQUAL((Uint4)2, query_v[0].genetic_code_id);

    GenCodeSingletonFini();
}

BOOST_AUTO_TEST_CASE(testMainSetup)
{
    CSeq_id id("gi|6648925");
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const ENa_strand kStrand = eNa_strand_both;

    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id,
                                                    kStrand));
    TSeqLocVector query_v;
    query_v.push_back(*sl);

    CBlastQueryInfo query_info;
    SetupQueryInfo(query_v, kProgram, kStrand, &query_info);

    TSearchMessages msgs;
    Blast_Message* blast_msg=NULL;

    CBLAST_SequenceBlk query_blk;
    SetupQueries(query_v, query_info, &query_blk, 
                    kProgram, kStrand, msgs);

    QuerySetUpOptions *qsup_opts = NULL;
    BlastQuerySetUpOptionsNew(&qsup_opts);

    BlastScoringOptions* score_opts = NULL;
    BlastScoringOptionsNew(kProgram, &score_opts);

    BlastSeqLoc *lookup_segments = NULL;
    BlastMaskLoc *mask = NULL;
    BlastScoreBlk *sbp = NULL;

    // supported options.
    score_opts->reward = 1;
    score_opts->penalty = -3;

    short st = BLAST_MainSetUp(kProgram, qsup_opts, score_opts, query_blk, query_info, 
            1.0, &lookup_segments, &mask, &sbp, &blast_msg, &BlastFindMatrixPath);

    BOOST_REQUIRE_EQUAL(0, (int) st);

    BOOST_REQUIRE_EQUAL(-3, sbp->matrix->data[1][11]);
    BOOST_REQUIRE_EQUAL(-2, sbp->matrix->data[0][11]);

    lookup_segments = BlastSeqLocFree(lookup_segments);
    mask = BlastMaskLocFree(mask);
    sbp = BlastScoreBlkFree(sbp);

    // supported options.
    score_opts->reward = 1;
    score_opts->penalty = -1;

    st = BLAST_MainSetUp(kProgram, qsup_opts, score_opts, query_blk, query_info, 
            1.0, &lookup_segments, &mask, &sbp, &blast_msg, &BlastFindMatrixPath);

    BOOST_REQUIRE_EQUAL(0, (int) st);

    BOOST_REQUIRE_EQUAL(-1, sbp->matrix->data[1][14]);
    BOOST_REQUIRE_EQUAL(-1, sbp->matrix->data[1][11]);
    BOOST_REQUIRE_EQUAL(0, sbp->matrix->data[0][11]);

    lookup_segments = BlastSeqLocFree(lookup_segments);
    mask = BlastMaskLocFree(mask);
    sbp = BlastScoreBlkFree(sbp);

    // NOT supported options.
    score_opts->reward = 1;
    score_opts->penalty = -3;
    score_opts->gap_open = 0;
    score_opts->gap_extend = 1;

    st = BLAST_MainSetUp(kProgram, qsup_opts, score_opts, query_blk, query_info, 
            1.0, &lookup_segments, &mask, &sbp, &blast_msg, &BlastFindMatrixPath);

    BOOST_REQUIRE_EQUAL(1, (int) st);

    lookup_segments = BlastSeqLocFree(lookup_segments);
    mask = BlastMaskLocFree(mask);
    sbp = BlastScoreBlkFree(sbp);

    // NOT supported options.
    score_opts->reward = 3124;
    score_opts->penalty = -4587;

    st = BLAST_MainSetUp(kProgram, qsup_opts, score_opts, query_blk, query_info, 
            1.0, &lookup_segments, &mask, &sbp, &blast_msg, &BlastFindMatrixPath);

    BOOST_REQUIRE_EQUAL(-1, (int) st);

    lookup_segments = BlastSeqLocFree(lookup_segments);
    mask = BlastMaskLocFree(mask);
    sbp = BlastScoreBlkFree(sbp);
    blast_msg = Blast_MessageFree(blast_msg);
    qsup_opts = BlastQuerySetUpOptionsFree(qsup_opts);
    score_opts = BlastScoringOptionsFree(score_opts);
}

BOOST_AUTO_TEST_CASE(testSetupWithZeroLengthSequence)
{
    CSeq_id id1("gi|6648925");
    CSeq_id id2("gi|405832");
    CSeq_id id3("gi|405834");

    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const ENa_strand kStrand = eNa_strand_both;

    auto_ptr<SSeqLoc> ssl1(CTestObjMgr::Instance().CreateSSeqLoc(id1, kStrand));
    auto_ptr<SSeqLoc> ssl2(CTestObjMgr::Instance().CreateEmptySSeqLoc(id2));
    auto_ptr<SSeqLoc> ssl3(CTestObjMgr::Instance().CreateSSeqLoc(id3, kStrand));

    CSeq_loc* mask_seqloc = new CSeq_loc(id3, 10, 20, kStrand);
    ssl3->mask.Reset(mask_seqloc);

    TSeqLocVector query_v;
    query_v.push_back(*ssl1);
    query_v.push_back(*ssl2);
    query_v.push_back(*ssl3);

    CBlastQueryInfo query_info;
    SetupQueryInfo(query_v, kProgram, kStrand, &query_info);

    TSearchMessages blast_msg;
    CBLAST_SequenceBlk query_blk;
    SetupQueries(query_v, query_info, &query_blk, 
                    kProgram, kStrand, blast_msg);

    
    BlastMaskLoc* lcase_mask = query_blk.Get()->lcase_mask;
    BOOST_REQUIRE(lcase_mask != NULL);
    BOOST_REQUIRE(lcase_mask->seqloc_array != NULL);
    BOOST_REQUIRE(lcase_mask->seqloc_array[2] == NULL);
    BOOST_REQUIRE(lcase_mask->seqloc_array[3] == NULL);
    BOOST_REQUIRE(lcase_mask->seqloc_array[4] != NULL);
    BOOST_REQUIRE(lcase_mask->seqloc_array[4]->ssr != NULL);
    BOOST_REQUIRE_EQUAL(10, lcase_mask->seqloc_array[4]->ssr->left); 
    BOOST_REQUIRE_EQUAL(20, lcase_mask->seqloc_array[4]->ssr->right); 
    string msg;
    ITERATE(TSearchMessages, m, blast_msg) {
        ITERATE(TQueryMessages, qm, *m) {
            msg += (*qm)->GetMessage();
        }
    }
    BOOST_REQUIRE(msg.find("Sequence contains no data") != string::npos);
}

BOOST_AUTO_TEST_CASE(GetSubjectTranslations)
{
    CSeq_id id("gi|6648925");
    const EBlastEncoding kEncoding = eBlastEncodingNcbi4na;

    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id, 
                                                    eNa_strand_both));

    string warnings;
    SBlastSequence nucl_seq(GetSequence(*sl->seqloc, kEncoding, 
                                        sl->scope, eNa_strand_both, 
                                        eSentinels, &warnings));

    Uint1* translation_buffer = NULL;
    Uint1* mixed_seq = NULL;
    Int4* frame_offsets = NULL;
    int nucl_length = nucl_seq.length;

    BLAST_GetAllTranslations(nucl_seq.data.get(), kEncoding, nucl_seq.length,
                                FindGeneticCode(1).get(), &translation_buffer, 
                                &frame_offsets, &mixed_seq);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[0]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[1]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[2]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[nucl_length+1]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[nucl_length+2]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[nucl_length+3]);
    BOOST_REQUIRE_EQUAL(0, (int) mixed_seq[2*nucl_length+2]);

    for (int index = 0; index <= NUM_FRAMES; ++index) {
        BOOST_REQUIRE_EQUAL(0, 
            (int) translation_buffer[frame_offsets[index]]);
    }
    BOOST_REQUIRE_EQUAL(0, (int)frame_offsets[0]);
    BOOST_REQUIRE_EQUAL(nucl_length+1, (int)frame_offsets[3]);
    BOOST_REQUIRE_EQUAL(2*nucl_length+2, (int)frame_offsets[NUM_FRAMES]);

    sfree(translation_buffer);
    sfree(frame_offsets);
    sfree(mixed_seq);
}

BOOST_AUTO_TEST_CASE(GetMixedFrameQuery)
{
    CSeq_id id("gi|6648925");
    const EProgram kProgram = eBlastx;

    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id,
                                                    eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*sl);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(kProgram));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                    prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    BLAST_CreateMixedFrameDNATranslation(query_blk, query_info);

    BOOST_REQUIRE(query_blk->oof_sequence != NULL);
    for (int index = 0; index < NUM_FRAMES; index += CODON_LENGTH) {
        int offset = query_info->contexts[index].query_offset;
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset]);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset+1]);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset+2]);
    }
    BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[query_blk->length+1]);
}

BOOST_AUTO_TEST_CASE(GetMixedFrameQueryOnOneStrand)
{
    CSeq_id id("gi|6648925");
    const EProgram kProgram = eBlastx;

    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id,
                                                    eNa_strand_plus));
    TSeqLocVector query_v;
    query_v.push_back(*sl);

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    TSearchMessages blast_msg;

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(kProgram));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    SetupQueries(query_v, query_info, &query_blk, 
                    prog, strand_opt, blast_msg);
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }

    BLAST_CreateMixedFrameDNATranslation(query_blk, query_info);

    BOOST_REQUIRE(query_blk->oof_sequence != NULL);
    for (int index = 0; index < CODON_LENGTH; index += CODON_LENGTH) {
        int offset = query_info->contexts[index].query_offset;
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset]);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset+1]);
        BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[offset+2]);
    }
    BOOST_REQUIRE_EQUAL(0, (int)query_blk->oof_sequence[query_blk->length+1]);
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: blastsetup-cppunit.cpp,v $
* Revision 1.84  2009/05/29 17:14:52  camacho
* Fix compiler error
*
* Revision 1.83  2009/05/29 15:15:24  camacho
* ePSITblastn is now supported JIRA SB-237
*
* Revision 1.82  2009/02/02 21:02:54  camacho
* Fix hash value
*
* Revision 1.81  2009/01/08 14:51:38  camacho
* Commit changes that go along JIRA SB-81
*
* Revision 1.80  2008/10/27 16:41:58  camacho
* Minor fix
*
* Revision 1.79  2008/04/15 13:50:29  madden
* Update tests for svn 124499
*
* Revision 1.78  2008/01/18 21:02:23  camacho
* Fix to match SVN revision 117705
*
* Revision 1.77  2007/07/25 12:41:39  madden
* Accomodates changes to blastn type defaults
*
* Revision 1.76  2007/03/22 14:34:44  camacho
* + support for auto-detection of genetic codes
*
* Revision 1.75  2007/03/20 14:54:02  camacho
* changes related to addition of multiple genetic code specification
*
* Revision 1.74  2007/01/22 14:55:32  camacho
* + test for ungapped effective search space calculation
*
* Revision 1.73  2006/06/05 13:34:05  madden
* Changes to remove [GS]etMatrixPath and use callback instead
*
* Revision 1.72  2006/05/24 17:23:12  madden
* Replace FindMatrixPath with FindMatrixOrPath
*
* Revision 1.71  2006/05/18 16:30:56  papadopo
* 1. Change signature of BLAST_CalcEffLengths
* 2. Do not set search space field directly
*
* Revision 1.70  2006/05/08 17:27:46  camacho
* Fix memory leak
*
* Revision 1.69  2006/05/05 22:17:27  camacho
* + test to ensure there are no side effects when using the search space calculator
*
* Revision 1.68  2006/05/05 17:46:15  camacho
* Rename BLAST_InitDNAPSequence to BLAST_CreateMixedFrameDNATranslation
*
* Revision 1.67  2006/04/25 16:09:28  camacho
* + test for CEffectiveSearchSpaceCalculator
*
* Revision 1.66  2006/04/20 19:36:26  madden
* NULL out Blast_Message when declared
*
* Revision 1.65  2006/04/03 13:14:23  madden
* New test GetMixedFrameQueryOnOneStrand for one stranded OOF setup
*
* Revision 1.64  2006/01/30 17:42:25  bealer
* - Add BOOST_REQUIRE to check for fatal errors ahead of time, and use
*   same in place of BOOST_REQUIRE() in others.
*
* Revision 1.63  2006/01/12 20:42:51  camacho
* Fix calls to BLAST_MainSetUp to include Blast_Message argument, use BlastQueryInfoNew
*
* Revision 1.62  2005/12/16 20:51:50  camacho
* Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
*
* Revision 1.61  2005/11/10 14:47:31  madden
* Add testSetupWithZeroLengthSequence
*
* Revision 1.60  2005/10/25 14:20:37  camacho
* repeats_filter.hpp and dust_filter.hpp are now private headers
*
* Revision 1.59  2005/10/14 13:47:32  camacho
* Fixes to pacify icc compiler
*
* Revision 1.58  2005/10/03 12:57:40  madden
* Add testMainSetup to test BLAST_MainSetUp from blast_setup.c
*
* Revision 1.57  2005/09/23 19:46:58  camacho
* + SubjectIsProtein test
*
* Revision 1.56  2005/09/23 18:55:53  camacho
* + overloaded CTestObjMgr::CreateSSeqLoc
*
* Revision 1.55  2005/09/23 14:23:28  camacho
* + tests to determine if subject is nucleotide
*
* Revision 1.54  2005/09/20 00:38:26  camacho
* Minor fix
*
* Revision 1.53  2005/09/12 20:57:53  camacho
* + tests for blast_program.h and for BlastNumber2Program
*
* Revision 1.52  2005/09/02 16:05:34  camacho
* Rename GetNumberOfFrames -> GetNumberOfContexts
*
* Revision 1.51  2005/08/15 16:14:14  dondosha
* Changed some values due to new blastn statistics
*
* Revision 1.50  2005/06/09 20:37:05  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.49  2005/05/24 20:05:17  camacho
* Changed signature of SetupQueries and SetupQueryInfo
*
* Revision 1.48  2005/05/10 16:09:03  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.47  2005/04/27 20:08:40  dondosha
* PHI-blast boolean argument has been removed from BlastSetup_ScoreBlkInit
*
* Revision 1.46  2005/04/06 21:27:18  dondosha
* Use EBlastProgramType instead of EProgram in internal functions
*
* Revision 1.45  2005/03/07 21:46:19  dondosha
* Added tests for set up of a mixed frame sequence
*
* Revision 1.44  2005/03/04 17:20:44  bealer
* - Command line option support.
*
* Revision 1.43  2005/02/10 21:00:00  dondosha
* Small memory leak fix
*
* Revision 1.42  2005/01/06 16:08:33  camacho
* + GetSequenceProteinWithSelenocysteine unit test
*
* Revision 1.41  2005/01/06 15:43:25  camacho
* Make use of modified signature to blast::SetupQueries
*
* Revision 1.40  2004/12/28 16:48:26  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.39  2004/12/22 16:06:08  dondosha
* Fixed memory leaks
*
* Revision 1.38  2004/12/21 17:02:46  dondosha
* Correction to testCalcEffLengthsWithUserSearchSpace unit test, to allow length adjustment to be calculated
*
* Revision 1.37  2004/12/21 15:15:22  dondosha
* Rewrote testCalcEffLengths test to use higher level API; added testCalcEffLengthsWithUserSearchSpace test to check BLAST_CalcEffLength function for a special condition
*
* Revision 1.36  2004/12/20 20:27:03  camacho
* + PSI-BLAST tests to BLAST_ContextToFrame
*
* Revision 1.35  2004/12/09 15:25:35  dondosha
* Ideal KA parameters calculation moved out of Blast_ScoreBlkMatrixInit
*
* Revision 1.34  2004/12/06 22:28:41  bealer
* - Fix array sizes.
*
* Revision 1.33  2004/12/06 22:21:21  bealer
* - Move RPS-tblastn to its rightful home in the list of translated searches.
*
* Revision 1.32  2004/12/06 22:15:46  camacho
* Fix to previous commit
*
* Revision 1.31  2004/12/06 21:42:45  camacho
* Display integer values in errors for Context2Frame unit tests
*
* Revision 1.30  2004/12/02 20:30:36  camacho
* + unit tests for BLAST_ContextToFrame
*
* Revision 1.29  2004/12/02 16:38:24  bealer
* - Change multiple-arrays to array-of-struct in BlastQueryInfo
*
* Revision 1.28  2004/12/01 22:02:14  camacho
* Remove dead code
*
* Revision 1.27  2004/12/01 20:33:29  dondosha
* Renamed constant variables according to toolkit convention
*
* Revision 1.26  2004/11/23 21:49:43  camacho
* Rename Blast_KarlinBlk* allocation/deallocation structures to follow standard naming conventions.
*
* Revision 1.25  2004/11/12 16:52:48  camacho
* Add tests which throw exceptions for ProgramNameToEnum
*
* Revision 1.24  2004/11/12 16:45:29  camacho
* 1. Added unit tests for ProgramNameToEnum
* 2. Added tests for missing EProgram values in GetNumberOfFrames tests
*
* Revision 1.23  2004/10/14 16:08:47  madden
* Fix UMR detected in testCalcEffLengths by valgrind
*
* Revision 1.22  2004/09/21 13:53:43  dondosha
* Added test for the BLAST_CalcEffLengths routine for all programs
*
* Revision 1.21  2004/07/12 21:53:38  camacho
* Do not clear the metaregistry's search paths.
*
* Revision 1.20  2004/07/06 21:56:27  camacho
* Ensure that an exception is thrown for invalid program type
*
* Revision 1.19  2004/06/21 20:11:22  camacho
* Fix memory leaks
*
* Revision 1.18  2004/06/14 17:48:55  madden
* Added tests GetGapOnlySequenceNCBI2NA and GetGapInSequenceNCBI2NA to check that subject sequence correctly retrieved when it contains a gap
*
* Revision 1.17  2004/03/23 16:10:34  camacho
* Minor changes to CTestObjMgr
*
* Revision 1.16  2004/03/15 20:00:56  dondosha
* SetupSubjects prototype changed to take just program instead of CBlastOptions*
*
* Revision 1.15  2004/03/06 00:40:27  camacho
* Use correct enum argument to ncbi::blast::GetSequence
*
* Revision 1.14  2004/02/20 23:20:36  camacho
* Remove undefs.h
*
* Revision 1.13  2004/01/08 17:20:09  camacho
* Change assertions to pacify gcc on darwin
*
* Revision 1.12  2003/12/12 16:23:33  camacho
* Minor
*
* Revision 1.11  2003/12/09 18:20:57  camacho
* Use BOOST_REQUIRE_EQUAL whenever possible
*
* Revision 1.10  2003/11/26 20:17:23  camacho
* Minor changes
*
* Revision 1.9  2003/11/26 18:43:18  camacho
* +Blast Option Handle classes
*
* Revision 1.8  2003/11/13 18:53:30  camacho
* Include file with #undefs to avoid compiler warnings
*
* Revision 1.7  2003/10/31 01:05:53  camacho
* Added unit tests for ncbi::blast::GetSequence
*
* Revision 1.6  2003/10/21 02:38:17  camacho
* Use TSeqPos instead of ints for sequence ranges
*
* Revision 1.5  2003/10/20 21:31:46  camacho
* Added log history
*
*
* ===========================================================================
*/
