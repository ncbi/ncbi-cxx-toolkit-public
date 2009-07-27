/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *   thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *   and reliability of the software and data, the NLM and the U.S.
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Unit test for CSeq_loc_Mapper_Base, CSeq_align_Mapper_Base and
 *   some closely related code.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>
#include <objects/seq/seq_align_mapper_base.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);


#define CHECK_GI(id, gi) \
    BOOST_CHECK_EQUAL((id).Which(), CSeq_id::e_Gi); \
    BOOST_CHECK_EQUAL((id).GetGi(), gi);

#define CHECK_SEQ_INT(loc, gi, from, to, \
                      have_strand, strand, \
                      fuzz_from, fuzz_to) \
    CHECK_GI((loc).GetId(), gi); \
    BOOST_CHECK_EQUAL((loc).GetFrom(), from); \
    BOOST_CHECK_EQUAL((loc).GetTo(), to); \
    BOOST_CHECK_EQUAL((loc).IsSetStrand(), have_strand); \
    if (have_strand) { \
        BOOST_CHECK_EQUAL((loc).GetStrand(), strand); \
    } \
    if (fuzz_from != CInt_fuzz::eLim_unk) { \
        BOOST_CHECK((loc).IsSetFuzz_from()); \
        BOOST_CHECK_EQUAL((loc).GetFuzz_from().Which(), CInt_fuzz::e_Lim); \
        BOOST_CHECK_EQUAL((loc).GetFuzz_from().GetLim(), fuzz_from); \
    } \
    else { \
        BOOST_CHECK(!(loc).IsSetFuzz_from()); \
    } \
    if (fuzz_to != CInt_fuzz::eLim_unk) { \
        BOOST_CHECK((loc).IsSetFuzz_to()); \
        BOOST_CHECK_EQUAL((loc).GetFuzz_to().Which(), CInt_fuzz::e_Lim); \
        BOOST_CHECK_EQUAL((loc).GetFuzz_to().GetLim(), fuzz_to); \
    } \
    else { \
        BOOST_CHECK(!(loc).IsSetFuzz_to()); \
    }

// Workaround for internal compiler error on MSVC7 with using original
// kInvalidSeqPos with BOOST_CHECK_EQUAL.
#if NCBI_COMPILER_MSVC && (_MSC_VER < 1400) // 1400 == VC++ 8.0 
#   undef  kInvalidSeqPos
#   define kInvalidSeqPos  -1
#endif


NCBITEST_AUTO_INIT()
{
}


NCBITEST_AUTO_FINI()
{
}


static void s_InitInterval(CSeq_interval& loc,
                             int gi,
                             TSeqPos from,
                             TSeqPos to,
                             ENa_strand strand = eNa_strand_unknown)
{
    loc.SetId().SetGi(gi);
    loc.SetFrom(from);
    loc.SetTo(to);
    if (strand != eNa_strand_unknown) {
        loc.SetStrand(strand);
    }
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Simple)
{
    // Basic mapping
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 110, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 89);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 115, 189, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 109);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 115, 199, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}

    {{
        // Minus strand interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 89, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 115, 189, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 109, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 115, 199, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Reverse)
{
    // Mapping through minus strand
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 110, 199, eNa_strand_minus);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 89);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 120, 194, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 109);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 110, 194, true, eNa_strand_minus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 89, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 120, 194, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 109, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 110, 194, true, eNa_strand_plus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_ProtToNuc)
{
    // Mapping from protein to nucleotide
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 6, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 120, 389);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 6, 15, 29);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 135, 179, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 6, 15, 119);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 135, 389, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}

    {{
        // Minus strand interval
        CSeq_loc loc;
        // This is not a real-life interval. Protein strand can not be negative.
        // But let's test it anyway.
        s_InitInterval(loc.SetInt(), 6, 15, 29, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 135, 179, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial on the right
        CSeq_loc loc;
        // This is not a real-life interval. Protein strand can not be negative.
        s_InitInterval(loc.SetInt(), 6, 15, 119, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 135, 389, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_ProtToNuc_Reverse)
{
    // Mapping from protein to nucleotide through minus strand
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 6, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 120, 389, eNa_strand_minus);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 6, 15, 59);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 240, 374, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 6, 15, 109);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 120, 374, true, eNa_strand_minus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand interval
        CSeq_loc loc;
        // This is not a real-life interval. Protein strand can not be negative.
        // But let's test it anyway.
        s_InitInterval(loc.SetInt(), 6, 15, 29, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 330, 374, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial on the right
        CSeq_loc loc;
        // This is not a real-life interval. Protein strand can not be negative.
        s_InitInterval(loc.SetInt(), 6, 15, 119, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 5, 120, 374, true, eNa_strand_plus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_NucToProt)
{
    // Mapping from nucleotide to protein
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 5, 30, 329);
    s_InitInterval(dst.SetInt(), 6, 100, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 89);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 6, 105, 119, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 359);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 6, 105, 199, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}

    {{
        // Minus strand interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 89, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein strand can not be negative!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 105, 119, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 359, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein strand can not be negative!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 105, 199, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}

    {{
        // Shifted nucleotide positions
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 46, 88);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped && mapped->IsInt());
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetInt(), 6, 105, 119, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_NucToProt_Reverse)
{
    // Mapping from nucleotide to protein through minus strand
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 5, 30, 329, eNa_strand_minus);
    s_InitInterval(dst.SetInt(), 6, 100, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 89);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein strand can not be negative!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 180, 194, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 359);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein strand can not be negative!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 100, 194, true, eNa_strand_minus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 89, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein can not have strand!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 180, 194, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Minus strand, partial on the right
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 45, 359, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein can not have strand!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 100, 194, true, eNa_strand_plus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
    }}

    {{
        // Simple interval
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 5, 46, 88);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Int);
        // !!! This is wrong! Protein strand can not be negative!
        CHECK_SEQ_INT(mapped->GetInt(), 6, 180, 194, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Mix)
{
    // Mapping through a set of ranges
    CSeq_loc src, dst;
    {{
        CRef<CSeq_loc> sub;
        // Source:
        // a) 10-19
        // b) 30-39
        // c) 50-59
        // d) 70-79
        for (TSeqPos p = 10; p < 80; p += 20) {
            sub = new CSeq_loc;
            s_InitInterval(sub->SetInt(), 4, p, p + 9);
            src.SetMix().Set().push_back(sub);
        }
        // Destination:
        // (a) => 100-110
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 100, 109);
        dst.SetMix().Set().push_back(sub);
        // (b) + start of (c) => 200-214
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 200, 214);
        dst.SetMix().Set().push_back(sub);
        // end of (c) + (d) => 300-314
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 300, 314);
        dst.SetMix().Set().push_back(sub);
    }}
    CSeq_loc_Mapper_Base mapper(src, dst);
    mapper.SetMergeAbutting();

    {{
        // Simple interval, overlap all source ranges
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 0, 89);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Packed_int);
        BOOST_CHECK_EQUAL(mapped->GetPacked_int().Get().size(), 3);
        CPacked_seqint::Tdata::const_iterator it =
            mapped->GetPacked_int().Get().begin();
        // Check all ranges
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 100, 109, false, eNa_strand_unknown,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_gt);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 200, 214, false, eNa_strand_unknown,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 300, 314, false, eNa_strand_unknown,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}

    {{
        // Simple interval, partial overlap, minus strand
        CSeq_loc loc;
        s_InitInterval(loc.SetInt(), 4, 15, 73, eNa_strand_minus);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Packed_int);
        BOOST_CHECK_EQUAL(mapped->GetPacked_int().Get().size(), 3);
        CPacked_seqint::Tdata::const_iterator it =
            mapped->GetPacked_int().Get().begin();
        // Check all ranges
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 300, 308, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 200, 214, true, eNa_strand_minus,
            CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 105, 109, true, eNa_strand_minus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_gt);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Mix_Reverse_Order)
{
    // Mapping through a set of ranges, check the order of mapped ranges
    CSeq_loc src, dst;
    {{
        CRef<CSeq_loc> sub;
        // Source:
        // a) 10-19
        // b) 30-49
        // c) 60-89
        TSeqPos len = 10;
        for (TSeqPos p = 10; p <= 60; p += len) {
            sub = new CSeq_loc;
            s_InitInterval(sub->SetInt(), 4, p, p + len - 1, eNa_strand_minus);
            len += 10;
            src.SetMix().Set().push_front(sub);
        }
    }}
    // Destination - single interval on plus strand
    s_InitInterval(dst.SetInt(), 5, 100, 159);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Original sea-loc is the same as mapping source
        CSeq_loc loc;
        loc.Assign(src);
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Packed_int);
        BOOST_CHECK_EQUAL(mapped->GetPacked_int().Get().size(), 3);
        CPacked_seqint::Tdata::const_iterator it =
            mapped->GetPacked_int().Get().begin();
        // Check all ranges
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 100, 129, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 130, 149, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 150, 159, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Map packed-int, check the order of intervals
        CSeq_loc loc;
        ITERATE(CSeq_loc_mix::Tdata, mit, src.GetMix().Get()) {
            CRef<CSeq_interval> si(new CSeq_interval);
            si->Assign((*mit)->GetInt());
            loc.SetPacked_int().Set().push_back(si);
        }
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Packed_int);
        BOOST_CHECK_EQUAL(mapped->GetPacked_int().Get().size(), 3);
        CPacked_seqint::Tdata::const_iterator it =
            mapped->GetPacked_int().Get().begin();
        // Check all ranges
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 100, 129, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 130, 149, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        ++it;
        BOOST_CHECK(*it);
        CHECK_SEQ_INT(**it, 5, 150, 159, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}

    {{
        // Worst case: non-plain mix
        CSeq_loc loc;
        {{
            CRef<CSeq_loc> sub1;
            // Source:
            // a) 10-19
            // b) 30-49
            // c) 60-89
            sub1 = new CSeq_loc;
            s_InitInterval(sub1->SetInt(), 4, 10, 11, eNa_strand_minus);
            loc.SetMix().Set().push_front(sub1);
            sub1 = new CSeq_loc;
            {{
                CRef<CSeq_loc> sub2;
                sub2 = new CSeq_loc;
                s_InitInterval(sub2->SetInt(), 4, 12, 14, eNa_strand_minus);
                sub1->SetMix().Set().push_front(sub2);
                sub2 = new CSeq_loc;
                s_InitInterval(sub2->SetInt(), 4, 15, 18, eNa_strand_minus);
                sub1->SetMix().Set().push_front(sub2);
            }}
            loc.SetMix().Set().push_front(sub1);
            sub1 = new CSeq_loc;
            {{
                CRef<CSeq_loc> sub2;
                sub2 = new CSeq_loc;
                {{
                    CRef<CSeq_loc> sub3;
                    sub3 = new CSeq_loc;
                    s_InitInterval(sub3->SetInt(), 4, 30, 31, eNa_strand_minus);
                    sub2->SetMix().Set().push_front(sub3);
                    sub3 = new CSeq_loc;
                    s_InitInterval(sub3->SetInt(), 4, 35, 37, eNa_strand_minus);
                    sub2->SetMix().Set().push_front(sub3);
                }}
                sub1->SetMix().Set().push_front(sub2);
                sub2 = new CSeq_loc;
                {{
                    CRef<CSeq_interval> int3;
                    int3 = new CSeq_interval;
                    s_InitInterval(*int3, 4, 40, 44, eNa_strand_minus);
                    sub2->SetPacked_int().Set().push_front(int3);
                    int3 = new CSeq_interval;
                    s_InitInterval(*int3, 4, 45, 47, eNa_strand_minus);
                    sub2->SetPacked_int().Set().push_front(int3);
                }}
                sub1->SetMix().Set().push_front(sub2);
            }}
            loc.SetMix().Set().push_front(sub1);
        }}
        CRef<CSeq_loc> mapped = mapper.Map(loc);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->Which(), CSeq_loc::e_Mix);
        BOOST_CHECK_EQUAL(mapped->GetMix().Get().size(), 3);
        CSeq_loc_mix::Tdata::const_iterator it1 =
            mapped->GetMix().Get().begin();
        // Check all ranges
        BOOST_CHECK(*it1);
        BOOST_CHECK_EQUAL((*it1)->Which(), CSeq_loc::e_Mix);
        BOOST_CHECK_EQUAL((*it1)->GetMix().Get().size(), 3);
        {{
            CSeq_loc_mix::Tdata::const_iterator it2 =
                (*it1)->GetMix().Get().begin();
            BOOST_CHECK(*it2);
            BOOST_CHECK_EQUAL((*it2)->Which(), CSeq_loc::e_Int);
            CHECK_SEQ_INT((*it2)->GetInt(), 5, 132, 134, true, eNa_strand_plus,
                CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
            ++it2;
            BOOST_CHECK(*it2);
            BOOST_CHECK_EQUAL((*it2)->Which(), CSeq_loc::e_Int);
            CHECK_SEQ_INT((*it2)->GetInt(), 5, 135, 139, true, eNa_strand_plus,
                CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
            ++it2;
            BOOST_CHECK(*it2);
            BOOST_CHECK_EQUAL((*it2)->Which(), CSeq_loc::e_Packed_int);
            BOOST_CHECK_EQUAL((*it2)->GetPacked_int().Get().size(), 2);
            {{
                CPacked_seqint::Tdata::const_iterator it3 =
                    (*it2)->GetPacked_int().Get().begin();
                // Check all ranges
                BOOST_CHECK(*it3);
                CHECK_SEQ_INT(**it3, 5, 142, 144, true, eNa_strand_plus,
                    CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
                ++it3;
                BOOST_CHECK(*it3);
                CHECK_SEQ_INT(**it3, 5, 148, 149, true, eNa_strand_plus,
                    CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
            }}
        }}
        ++it1;
        BOOST_CHECK(*it1);
        BOOST_CHECK_EQUAL((*it1)->Which(), CSeq_loc::e_Packed_int);
        BOOST_CHECK_EQUAL((*it1)->GetPacked_int().Get().size(), 2);
        {{
            CPacked_seqint::Tdata::const_iterator it2 =
                (*it1)->GetPacked_int().Get().begin();
            // Check all ranges
            BOOST_CHECK(*it2);
            CHECK_SEQ_INT(**it2, 5, 151, 154, true, eNa_strand_plus,
                CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
            ++it2;
            BOOST_CHECK(*it2);
            CHECK_SEQ_INT(**it2, 5, 155, 157, true, eNa_strand_plus,
                CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
        }}
        ++it1;
        BOOST_CHECK(*it1);
        BOOST_CHECK_EQUAL((*it1)->Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT((*it1)->GetInt(), 5, 158, 159, true, eNa_strand_plus,
            CInt_fuzz::eLim_unk, CInt_fuzz::eLim_unk);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Dendiag)
{
    // Dense-diag mapping
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 110, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Single segment
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CSeq_align::C_Segs::TDendiag& diags = aln.SetSegs().SetDendiag();
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(2);
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        diag->SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(4); // will be mapped to gi 5
        diag->SetIds().push_back(id);
        diag->SetStarts().push_back(915);
        diag->SetStarts().push_back(15);
        diag->SetLen(70);
        diags.push_back(diag);

        CRef<CSeq_align> mapped = mapper.Map(aln);
        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDendiag());
        BOOST_CHECK_EQUAL(mapped->GetSegs().GetDendiag().size(), 1);
        const CDense_diag& md = **mapped->GetSegs().GetDendiag().begin();
        BOOST_CHECK_EQUAL(md.GetDim(), 2);
        BOOST_CHECK_EQUAL(md.GetLen(), 70);
        BOOST_CHECK(!md.IsSetStrands());

        BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
        CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
        CHECK_GI(**id_it, 3);
        id_it++;
        CHECK_GI(**id_it, 5);

        BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
        CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
        BOOST_CHECK_EQUAL(*start, 915);
        start++;
        BOOST_CHECK_EQUAL(*start, 115);
    }}

    {{
        // Single segment, partial mapping.
        // Should produce extra-segments with gaps.
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CSeq_align::C_Segs::TDendiag& diags = aln.SetSegs().SetDendiag();
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(2);
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        diag->SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(4); // will be mapped to gi 5
        diag->SetIds().push_back(id);
        diag->SetStarts().push_back(905);
        diag->SetStarts().push_back(5);
        diag->SetLen(110);
        diags.push_back(diag);

        CRef<CSeq_align> mapped = mapper.Map(aln);
        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDendiag());
        BOOST_CHECK_EQUAL(mapped->GetSegs().GetDendiag().size(), 3);
        CSeq_align::C_Segs::TDendiag::const_iterator ds_it =
            mapped->GetSegs().GetDendiag().begin();
        // Three segments total, first and last have gaps in the mapped row.
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 5);
            BOOST_CHECK(!md.IsSetStrands());

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 905);
            start++;
            BOOST_CHECK_EQUAL(*start, kInvalidSeqPos);
        }}
        ds_it++;
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 90);
            BOOST_CHECK(!md.IsSetStrands());

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 910);
            start++;
            BOOST_CHECK_EQUAL(*start, 110);
        }}
        ds_it++;
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 15);
            BOOST_CHECK(!md.IsSetStrands());

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 1000);
            start++;
            BOOST_CHECK_EQUAL(*start, kInvalidSeqPos);
        }}
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Reverse_Dendiag)
{
    // Dense-diag mapping through minus strand
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99, eNa_strand_minus);
    s_InitInterval(dst.SetInt(), 5, 110, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CSeq_align::C_Segs::TDendiag& diags = aln.SetSegs().SetDendiag();
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(2);
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        diag->SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(4); // will be mapped to gi 5
        diag->SetIds().push_back(id);
        diag->SetStarts().push_back(915);
        diag->SetStarts().push_back(15);
        diag->SetLen(70);
        diags.push_back(diag);

        CRef<CSeq_align> mapped = mapper.Map(aln);
        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDendiag());
        BOOST_CHECK_EQUAL(mapped->GetSegs().GetDendiag().size(), 1);
        const CDense_diag& md = **mapped->GetSegs().GetDendiag().begin();
        BOOST_CHECK_EQUAL(md.GetDim(), 2);
        BOOST_CHECK_EQUAL(md.GetLen(), 70);

        BOOST_CHECK(md.IsSetStrands());
        BOOST_CHECK_EQUAL(md.GetStrands().size(), 2);
        CDense_diag::TStrands::const_iterator str_it = md.GetStrands().begin();
        BOOST_CHECK_EQUAL(*str_it, eNa_strand_unknown);
        str_it++;
        BOOST_CHECK_EQUAL(*str_it, eNa_strand_minus);

        BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
        CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
        CHECK_GI(**id_it, 3);
        id_it++;
        CHECK_GI(**id_it, 5);

        BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
        CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
        BOOST_CHECK_EQUAL(*start, 915);
        start++;
        BOOST_CHECK_EQUAL(*start, 125);
    }}

    {{
        // Single segment, partial mapping.
        // Should produce extra-segments with gaps.
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CSeq_align::C_Segs::TDendiag& diags = aln.SetSegs().SetDendiag();
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(2);
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        diag->SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(4); // will be mapped to gi 5
        diag->SetIds().push_back(id);
        diag->SetStarts().push_back(905);
        diag->SetStarts().push_back(5);
        diag->SetLen(110);
        diags.push_back(diag);

        CRef<CSeq_align> mapped = mapper.Map(aln);
        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDendiag());
        BOOST_CHECK_EQUAL(mapped->GetSegs().GetDendiag().size(), 3);
        CSeq_align::C_Segs::TDendiag::const_iterator ds_it =
            mapped->GetSegs().GetDendiag().begin();
        // Three segments total, first and last have gaps in the mapped row.
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 5);
            BOOST_CHECK(!md.IsSetStrands());

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 905);
            start++;
            BOOST_CHECK_EQUAL(*start, kInvalidSeqPos);
        }}
        ds_it++;
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 90);

            BOOST_CHECK(md.IsSetStrands());
            BOOST_CHECK_EQUAL(md.GetStrands().size(), 2);
            CDense_diag::TStrands::const_iterator str_it = md.GetStrands().begin();
            BOOST_CHECK_EQUAL(*str_it, eNa_strand_unknown);
            str_it++;
            BOOST_CHECK_EQUAL(*str_it, eNa_strand_minus);

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 910);
            start++;
            BOOST_CHECK_EQUAL(*start, 110);
        }}
        ds_it++;
        {{
            const CDense_diag& md = **ds_it;
            BOOST_CHECK_EQUAL(md.GetDim(), 2);
            BOOST_CHECK_EQUAL(md.GetLen(), 15);
            BOOST_CHECK(!md.IsSetStrands());

            BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
            CDense_diag::TIds::const_iterator id_it = md.GetIds().begin();
            CHECK_GI(**id_it, 3);
            id_it++;
            CHECK_GI(**id_it, 5);

            BOOST_CHECK_EQUAL(md.GetStarts().size(), 2);
            CDense_diag::TStarts::const_iterator start = md.GetStarts().begin();
            BOOST_CHECK_EQUAL(*start, 1000);
            start++;
            BOOST_CHECK_EQUAL(*start, kInvalidSeqPos);
        }}
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Nuc2Prot_Denseg)
{
    // Dense-seg mapping, nuc to prot
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 30, 89);
    s_InitInterval(dst.SetInt(), 6, 110, 129);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CDense_seg& dseg = aln.SetSegs().SetDenseg();
        dseg.SetDim(2);
        dseg.SetNumseg(1);

        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        dseg.SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(4); // will be mapped to gi 6
        dseg.SetIds().push_back(id);
        dseg.SetStarts().push_back(915);
        dseg.SetStarts().push_back(15);
        dseg.SetLens().push_back(90); // Extra bases on left and right

        CRef<CSeq_align> mapped = mapper.Map(aln);

        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDenseg());
        const CDense_seg& md = mapped->GetSegs().GetDenseg();
        BOOST_CHECK_EQUAL(md.GetDim(), 2);
        BOOST_CHECK(!md.IsSetStrands());
        BOOST_CHECK_EQUAL(md.GetNumseg(), 3);

        BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
        CDense_seg::TIds::const_iterator id_it = md.GetIds().begin();
        CHECK_GI(**id_it, 3);
        id_it++;
        CHECK_GI(**id_it, 6);

        BOOST_CHECK_EQUAL(md.GetStarts().size(), 6);
        CDense_seg::TStarts::const_iterator start = md.GetStarts().begin();
        BOOST_CHECK_EQUAL(*start, 915);
        start++;
        BOOST_CHECK_EQUAL(*start, -1);
        start++;
        BOOST_CHECK_EQUAL(*start, 930);
        start++;
        BOOST_CHECK_EQUAL(*start, 110);
        start++;
        BOOST_CHECK_EQUAL(*start, 990);
        start++;
        BOOST_CHECK_EQUAL(*start, -1);

        BOOST_CHECK_EQUAL(md.GetLens().size(), 3);
        CDense_seg::TLens::const_iterator len = md.GetLens().begin();
        BOOST_CHECK_EQUAL(*len, 15);
        len++;
        BOOST_CHECK_EQUAL(*len, 60);
        len++;
        BOOST_CHECK_EQUAL(*len, 15);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Prot2Nuc_Denseg)
{
    // Dense-seg mapping, prot to nuc
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 6, 30, 49);
    s_InitInterval(dst.SetInt(), 4, 110, 169);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        CSeq_align aln;
        aln.SetDim(2);
        aln.SetType(CSeq_align::eType_diags);
        CDense_seg& dseg = aln.SetSegs().SetDenseg();
        dseg.SetDim(2);
        dseg.SetNumseg(1);

        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(3); // not to be mapped
        dseg.SetIds().push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(6); // will be mapped to gi 4
        dseg.SetIds().push_back(id);
        dseg.SetStarts().push_back(915);
        dseg.SetStarts().push_back(15);
        dseg.SetLens().push_back(50); // Extra bases on left and right
        // Mapper treats all coordinates as nucleotide by default.
        // Need to specify widths to force protein coordinates in the
        // alignment row 2.
        dseg.SetWidths().push_back(1);
        dseg.SetWidths().push_back(3);

        CRef<CSeq_align> mapped = mapper.Map(aln);

        BOOST_CHECK(mapped);
        BOOST_CHECK(mapped->GetSegs().IsDenseg());
        const CDense_seg& md = mapped->GetSegs().GetDenseg();
        BOOST_CHECK_EQUAL(md.GetDim(), 2);
        BOOST_CHECK(!md.IsSetStrands());
        BOOST_CHECK_EQUAL(md.GetNumseg(), 3);

        BOOST_CHECK_EQUAL(md.GetIds().size(), 2);
        CDense_seg::TIds::const_iterator id_it = md.GetIds().begin();
        CHECK_GI(**id_it, 3);
        id_it++;
        CHECK_GI(**id_it, 4);

        BOOST_CHECK_EQUAL(md.GetStarts().size(), 6);
        CDense_seg::TStarts::const_iterator start = md.GetStarts().begin();
        BOOST_CHECK_EQUAL(*start, 915);
        start++;
        BOOST_CHECK_EQUAL(*start, -1);
        start++;
        BOOST_CHECK_EQUAL(*start, 960);
        start++;
        BOOST_CHECK_EQUAL(*start, 110);
        start++;
        BOOST_CHECK_EQUAL(*start, 1020);
        start++;
        BOOST_CHECK_EQUAL(*start, -1);

        BOOST_CHECK_EQUAL(md.GetLens().size(), 3);
        CDense_seg::TLens::const_iterator len = md.GetLens().begin();
        BOOST_CHECK_EQUAL(*len, 45);
        len++;
        BOOST_CHECK_EQUAL(*len, 60);
        len++;
        BOOST_CHECK_EQUAL(*len, 45);

        // Now both rows should be on nucs
        BOOST_CHECK_EQUAL(md.GetWidths().size(), 2);
        CDense_seg::TWidths::const_iterator wid = md.GetWidths().begin();
        BOOST_CHECK_EQUAL(*wid, 1);
        wid++;
        BOOST_CHECK_EQUAL(*wid, 1);
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_SplicedProd)
{
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99);
    s_InitInterval(dst.SetInt(), 5, 110, 199);
    CSeq_loc_Mapper_Base mapper(src, dst);
    CSeq_align aln;
    aln.SetType(CSeq_align::eType_global);
    aln.SetDim(2);
    {{
        CSpliced_seg& spl = aln.SetSegs().SetSpliced();
        spl.SetProduct_id().SetGi(4);
        spl.SetGenomic_id().SetGi(3);
        spl.SetProduct_strand(eNa_strand_plus);
        spl.SetGenomic_strand(eNa_strand_plus);
        spl.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spl.SetProduct_length(100);
        {{
            CRef<CSpliced_exon> ex(new CSpliced_exon);
            ex->SetProduct_start().SetNucpos(0);
            ex->SetProduct_end().SetNucpos(99);
            ex->SetGenomic_start(0);
            ex->SetGenomic_end(99);
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMatch(50);
                ex->SetParts().push_back(part);
            }}
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMismatch(50);
                ex->SetParts().push_back(part);
            }}
            CRef<CUser_object> ex_ext(new CUser_object);
            ex_ext->SetType().SetStr("exon-info");
            CRef<CUser_field> ex_fld(new CUser_field);
            ex_fld->SetLabel().SetStr("Whatever");
            ex_fld->SetData().SetInt(123);
            ex_ext->SetData().push_back(ex_fld);
            ex->SetExt().push_back(ex_ext);
            spl.SetExons().push_back(ex);
        }}
        CRef<CUser_object> ext(new CUser_object);
        ext->SetType().SetStr("info");
        CRef<CUser_field> fld(new CUser_field);
        fld->SetLabel().SetStr("Whatever");
        fld->SetData().SetInt(456);
        ext->SetData().push_back(fld);
        aln.SetExt().push_back(ext);
    }}
    CRef<CSeq_align> mapped = mapper.Map(aln);
    BOOST_CHECK(mapped);
    BOOST_CHECK(mapped->GetSegs().IsSpliced());
    const CSpliced_seg& spl = mapped->GetSegs().GetSpliced();
    CHECK_GI(spl.GetProduct_id(), 5);
    CHECK_GI(spl.GetGenomic_id(), 3);
    BOOST_CHECK_EQUAL(spl.GetProduct_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetGenomic_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetProduct_type(),
        CSpliced_seg::eProduct_type_transcript);
    BOOST_CHECK_EQUAL(spl.GetProduct_length(), 100);

    BOOST_CHECK_EQUAL(spl.GetExons().size(), 1);
    const CSpliced_exon& ex = **spl.GetExons().begin();
    BOOST_CHECK(ex.GetProduct_start().IsNucpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_start().GetNucpos(), 110);
    BOOST_CHECK(ex.GetProduct_end().IsNucpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_end().GetNucpos(), 199);
    BOOST_CHECK_EQUAL(ex.GetGenomic_start(), 0);
    BOOST_CHECK_EQUAL(ex.GetGenomic_end(), 99);
    BOOST_CHECK_EQUAL(ex.GetParts().size(), 3);
    CSpliced_exon::TParts::const_iterator part = ex.GetParts().begin();
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 40);
    part++;
    BOOST_CHECK((*part)->IsMismatch());
    BOOST_CHECK_EQUAL((*part)->GetMismatch(), 50);

    // Make sure exon's ext is copied, ignore the content
    BOOST_CHECK(ex.IsSetExt());
    BOOST_CHECK_EQUAL(ex.GetExt().size(), 1);

    // Make sure alignment's ext is copied, ignore the content
    BOOST_CHECK(mapped->IsSetExt());
    BOOST_CHECK_EQUAL(mapped->GetExt().size(), 1);
}


BOOST_AUTO_TEST_CASE(s_TestMapping_SplicedProd_Nuc2Prot)
{
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99);
    s_InitInterval(dst.SetInt(), 6, 110, 139);
    CSeq_loc_Mapper_Base mapper(src, dst);
    CSeq_align aln;
    aln.SetType(CSeq_align::eType_global);
    aln.SetDim(2);
    {{
        CSpliced_seg& spl = aln.SetSegs().SetSpliced();
        spl.SetProduct_id().SetGi(4);
        spl.SetGenomic_id().SetGi(3);
        spl.SetProduct_strand(eNa_strand_plus);
        spl.SetGenomic_strand(eNa_strand_plus);
        spl.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spl.SetProduct_length(100);
        {{
            CRef<CSpliced_exon> ex(new CSpliced_exon);
            ex->SetProduct_start().SetNucpos(0);
            ex->SetProduct_end().SetNucpos(99);
            ex->SetGenomic_start(0);
            ex->SetGenomic_end(99);
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMatch(50);
                ex->SetParts().push_back(part);
            }}
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMismatch(50);
                ex->SetParts().push_back(part);
            }}
            spl.SetExons().push_back(ex);
        }}
    }}
    CRef<CSeq_align> mapped = mapper.Map(aln);
    BOOST_CHECK(mapped);
    BOOST_CHECK(mapped->GetSegs().IsSpliced());
    const CSpliced_seg& spl = mapped->GetSegs().GetSpliced();
    CHECK_GI(spl.GetProduct_id(), 6);
    CHECK_GI(spl.GetGenomic_id(), 3);
    BOOST_CHECK_EQUAL(spl.GetProduct_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetGenomic_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetProduct_type(),
        CSpliced_seg::eProduct_type_protein);
    // BOOST_CHECK_EQUAL(spl.GetProduct_length(), 30);

    BOOST_CHECK_EQUAL(spl.GetExons().size(), 1);
    const CSpliced_exon& ex = **spl.GetExons().begin();
    BOOST_CHECK(ex.GetProduct_start().IsProtpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_start().GetProtpos().GetAmin(), 110);
    BOOST_CHECK(ex.GetProduct_end().IsProtpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_end().GetProtpos().GetAmin(), 139);
    BOOST_CHECK_EQUAL(ex.GetGenomic_start(), 0);
    BOOST_CHECK_EQUAL(ex.GetGenomic_end(), 99);
    BOOST_CHECK_EQUAL(ex.GetParts().size(), 3);
    CSpliced_exon::TParts::const_iterator part = ex.GetParts().begin();
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 40);
    part++;
    BOOST_CHECK((*part)->IsMismatch());
    BOOST_CHECK_EQUAL((*part)->GetMismatch(), 50);
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Reverse_SplicedProd_Nuc2Prot_MinusProd)
{
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 99, eNa_strand_minus);
    s_InitInterval(dst.SetInt(), 6, 110, 139);
    CSeq_loc_Mapper_Base mapper(src, dst);
    CSeq_align aln;
    aln.SetType(CSeq_align::eType_global);
    aln.SetDim(2);
    {{
        CSpliced_seg& spl = aln.SetSegs().SetSpliced();
        spl.SetProduct_id().SetGi(4);
        spl.SetGenomic_id().SetGi(3);
        spl.SetProduct_strand(eNa_strand_minus);
        spl.SetGenomic_strand(eNa_strand_plus);
        spl.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spl.SetProduct_length(100);
        {{
            CRef<CSpliced_exon> ex(new CSpliced_exon);
            ex->SetProduct_start().SetNucpos(0);
            ex->SetProduct_end().SetNucpos(99);
            ex->SetGenomic_start(0);
            ex->SetGenomic_end(99);
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMatch(50);
                ex->SetParts().push_back(part);
            }}
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMismatch(50);
                ex->SetParts().push_back(part);
            }}
            spl.SetExons().push_back(ex);
        }}
    }}
    CRef<CSeq_align> mapped = mapper.Map(aln);
    BOOST_CHECK(mapped);
    BOOST_CHECK(mapped->GetSegs().IsSpliced());
    const CSpliced_seg& spl = mapped->GetSegs().GetSpliced();
    CHECK_GI(spl.GetProduct_id(), 6);
    CHECK_GI(spl.GetGenomic_id(), 3);
    BOOST_CHECK_EQUAL(spl.GetProduct_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetGenomic_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetProduct_type(),
        CSpliced_seg::eProduct_type_protein);
    // BOOST_CHECK_EQUAL(spl.GetProduct_length(), 30);
    
    BOOST_CHECK_EQUAL(spl.GetExons().size(), 1);
    const CSpliced_exon& ex = **spl.GetExons().begin();
    BOOST_CHECK(ex.GetProduct_start().IsProtpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_start().GetProtpos().GetAmin(), 110);
    BOOST_CHECK(ex.GetProduct_end().IsProtpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_end().GetProtpos().GetAmin(), 139);
    BOOST_CHECK_EQUAL(ex.GetGenomic_start(), 0);
    BOOST_CHECK_EQUAL(ex.GetGenomic_end(), 99);
    BOOST_CHECK_EQUAL(ex.GetParts().size(), 3);
    CSpliced_exon::TParts::const_iterator part = ex.GetParts().begin();
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 50);
    part++;
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMismatch());
    BOOST_CHECK_EQUAL((*part)->GetMismatch(), 40);
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Multirange_Spliced)
{
    CSeq_loc src, dst;
    {{
        CRef<CSeq_loc> sub;
        // Source:
        // a) 10-19
        // b) 30-39
        // c) 50-59
        // d) 70-79
        for (TSeqPos p = 10; p < 80; p += 20) {
            sub = new CSeq_loc;
            s_InitInterval(sub->SetInt(), 4, p, p + 9);
            src.SetMix().Set().push_back(sub);
        }
        // Destination:
        // (a) => 100-110
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 100, 109);
        dst.SetMix().Set().push_back(sub);
        // (b) + start of (c) => 200-214
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 200, 214);
        dst.SetMix().Set().push_back(sub);
        // end of (c) + (d) => 300-314
        sub = new CSeq_loc;
        s_InitInterval(sub->SetInt(), 5, 300, 314);
        dst.SetMix().Set().push_back(sub);
    }}
    CSeq_loc_Mapper_Base mapper(src, dst);
    CSeq_align aln;
    aln.SetType(CSeq_align::eType_global);
    aln.SetDim(2);
    {{
        CSpliced_seg& spl = aln.SetSegs().SetSpliced();
        spl.SetProduct_id().SetGi(4);
        spl.SetGenomic_id().SetGi(3);
        spl.SetProduct_strand(eNa_strand_plus);
        spl.SetGenomic_strand(eNa_strand_plus);
        spl.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spl.SetProduct_length(100);
        {{
            CRef<CSpliced_exon> ex(new CSpliced_exon);
            ex->SetProduct_start().SetNucpos(0);
            ex->SetProduct_end().SetNucpos(99);
            ex->SetGenomic_start(0);
            ex->SetGenomic_end(99);
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMatch(52);
                ex->SetParts().push_back(part);
            }}
            {{
                CRef<CSpliced_exon_chunk> part(new CSpliced_exon_chunk);
                part->SetMismatch(48);
                ex->SetParts().push_back(part);
            }}
            spl.SetExons().push_back(ex);
        }}
    }}
    CRef<CSeq_align> mapped = mapper.Map(aln);
    BOOST_CHECK(mapped);
    BOOST_CHECK(mapped->GetSegs().IsSpliced());
    const CSpliced_seg& spl = mapped->GetSegs().GetSpliced();
    CHECK_GI(spl.GetProduct_id(), 5);
    CHECK_GI(spl.GetGenomic_id(), 3);
    BOOST_CHECK_EQUAL(spl.GetProduct_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetGenomic_strand(), eNa_strand_plus);
    BOOST_CHECK_EQUAL(spl.GetProduct_type(),
        CSpliced_seg::eProduct_type_transcript);
    // BOOST_CHECK_EQUAL(spl.GetProduct_length(), 40);

    BOOST_CHECK_EQUAL(spl.GetExons().size(), 1);
    const CSpliced_exon& ex = **spl.GetExons().begin();
    BOOST_CHECK(ex.GetProduct_start().IsNucpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_start().GetNucpos(), 100);
    BOOST_CHECK(ex.GetProduct_end().IsNucpos());
    BOOST_CHECK_EQUAL(ex.GetProduct_end().GetNucpos(), 314);
    BOOST_CHECK_EQUAL(ex.GetGenomic_start(), 0);
    BOOST_CHECK_EQUAL(ex.GetGenomic_end(), 99);
    BOOST_CHECK_EQUAL(ex.GetParts().size(), 10);
    CSpliced_exon::TParts::const_iterator part = ex.GetParts().begin();
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 10);
    part++;
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 10);
    part++;
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMatch());
    BOOST_CHECK_EQUAL((*part)->GetMatch(), 2);
    part++;
    BOOST_CHECK((*part)->IsMismatch());
    BOOST_CHECK_EQUAL((*part)->GetMismatch(), 8);
    part++;
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 10);
    part++;
    BOOST_CHECK((*part)->IsMismatch());
    BOOST_CHECK_EQUAL((*part)->GetMismatch(), 10);
    part++;
    BOOST_CHECK((*part)->IsGenomic_ins());
    BOOST_CHECK_EQUAL((*part)->GetGenomic_ins(), 20);
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Graph_Simple)
{
    // Basic mapping
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 19);
    s_InitInterval(dst.SetInt(), 5, 110, 119);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple graph
        CSeq_graph graph;
        s_InitInterval(graph.SetLoc().SetInt(), 4, 8, 28);
        graph.SetNumval(20);
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 20; i++) {
            int_graph.SetValues().push_back(i);
        }
        CRef<CSeq_graph> mapped = mapper.Map(graph);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->GetLoc().Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetLoc().GetInt(), 5, 110, 119, false,
            eNa_strand_unknown, CInt_fuzz::eLim_lt, CInt_fuzz::eLim_gt);
        BOOST_CHECK_EQUAL(mapped->GetComp(), 1);
        BOOST_CHECK_EQUAL(mapped->GetNumval(), 10);
        BOOST_CHECK_EQUAL(mapped->GetGraph().Which(),
            CSeq_graph::C_Graph::e_Int);
        const CInt_graph& int_mapped = mapped->GetGraph().GetInt();
        BOOST_CHECK_EQUAL(int_mapped.GetMax(), 100);
        BOOST_CHECK_EQUAL(int_mapped.GetMin(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetAxis(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetValues().size(), 10);
        for (size_t i = 0; i < int_mapped.GetValues().size(); i++) {
            BOOST_CHECK_EQUAL(int_mapped.GetValues()[i], i + 2);
        }
    }}

    {{
        // Partial - skip a range in the middle
        CSeq_graph graph;
        {{
            CRef<CSeq_interval> loc_int(new CSeq_interval);
            s_InitInterval(*loc_int, 4, 5, 14);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
            loc_int.Reset(new CSeq_interval);
            s_InitInterval(*loc_int, 999, 105, 114);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
            loc_int.Reset(new CSeq_interval);
            s_InitInterval(*loc_int, 4, 15, 24);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
        }}
        graph.SetNumval(30);
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 30; i++) {
            int_graph.SetValues().push_back(i);
        }
        CRef<CSeq_graph> mapped = mapper.Map(graph);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->GetLoc().Which(), CSeq_loc::e_Packed_int);
        const CPacked_seqint::Tdata& data =
            mapped->GetLoc().GetPacked_int().Get();
        BOOST_CHECK_EQUAL(data.size(), 2);
        CPacked_seqint::Tdata::const_iterator it = data.begin();
        CHECK_SEQ_INT(**it, 5, 110, 114, false,
            eNa_strand_unknown, CInt_fuzz::eLim_lt, CInt_fuzz::eLim_tr);
        it++;
        CHECK_SEQ_INT(**it, 5, 115, 119, false,
            eNa_strand_unknown, CInt_fuzz::eLim_tl, CInt_fuzz::eLim_gt);
        BOOST_CHECK_EQUAL(mapped->GetComp(), 1);
        BOOST_CHECK_EQUAL(mapped->GetNumval(), 10);
        BOOST_CHECK_EQUAL(mapped->GetGraph().Which(),
            CSeq_graph::C_Graph::e_Int);
        const CInt_graph& int_mapped = mapped->GetGraph().GetInt();
        BOOST_CHECK_EQUAL(int_mapped.GetMax(), 100);
        BOOST_CHECK_EQUAL(int_mapped.GetMin(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetAxis(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetValues().size(), 10);
        for (size_t i = 0, val = 5; i < int_mapped.GetValues().size(); i++, val++) {
            if (i == 5) {
                val += 10;
            }
            BOOST_CHECK_EQUAL(int_mapped.GetValues()[i], val);
        }
    }}

    {{
        // Minus strand, the data should not be reversed
        CSeq_graph graph;
        s_InitInterval(graph.SetLoc().SetInt(), 4, 8, 28, eNa_strand_minus);
        graph.SetNumval(20);
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 20; i++) {
            int_graph.SetValues().push_back(i);
        }
        CRef<CSeq_graph> mapped = mapper.Map(graph);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->GetLoc().Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetLoc().GetInt(), 5, 110, 119, true,
            eNa_strand_minus, CInt_fuzz::eLim_lt, CInt_fuzz::eLim_gt);
        BOOST_CHECK_EQUAL(mapped->GetComp(), 1);
        BOOST_CHECK_EQUAL(mapped->GetNumval(), 10);
        BOOST_CHECK_EQUAL(mapped->GetGraph().Which(),
            CSeq_graph::C_Graph::e_Int);
        const CInt_graph& int_mapped = mapped->GetGraph().GetInt();
        BOOST_CHECK_EQUAL(int_mapped.GetMax(), 100);
        BOOST_CHECK_EQUAL(int_mapped.GetMin(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetAxis(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetValues().size(), 10);
        for (size_t i = 0; i < int_mapped.GetValues().size(); i++) {
            BOOST_CHECK_EQUAL(int_mapped.GetValues()[i], i + 2);
        }
    }}
}


BOOST_AUTO_TEST_CASE(s_TestMapping_Graph_Unsupported)
{
    // Basic mapping
    CSeq_loc src, dst;
    s_InitInterval(src.SetInt(), 4, 10, 39);
    s_InitInterval(dst.SetInt(), 6, 110, 119);
    CSeq_loc_Mapper_Base mapper(src, dst);

    {{
        // Simple graph, using comp=3 to allow mapping
        CSeq_graph graph;
        s_InitInterval(graph.SetLoc().SetInt(), 4, 7, 27);
        graph.SetNumval(20);
        graph.SetComp(3); // This allows to map nuc-to-prot
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 20; i++) {
            int_graph.SetValues().push_back(i);
        }
        CRef<CSeq_graph> mapped = mapper.Map(graph);
        BOOST_CHECK(mapped);
        BOOST_CHECK_EQUAL(mapped->GetLoc().Which(), CSeq_loc::e_Int);
        CHECK_SEQ_INT(mapped->GetLoc().GetInt(), 6, 110, 115, false,
            eNa_strand_unknown, CInt_fuzz::eLim_lt, CInt_fuzz::eLim_unk);
        BOOST_CHECK_EQUAL(mapped->GetComp(), 1);
        BOOST_CHECK_EQUAL(mapped->GetNumval(), 6);
        BOOST_CHECK_EQUAL(mapped->GetGraph().Which(),
            CSeq_graph::C_Graph::e_Int);
        const CInt_graph& int_mapped = mapped->GetGraph().GetInt();
        BOOST_CHECK_EQUAL(int_mapped.GetMax(), 100);
        BOOST_CHECK_EQUAL(int_mapped.GetMin(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetAxis(), 0);
        BOOST_CHECK_EQUAL(int_mapped.GetValues().size(), 6);
        for (size_t i = 0; i < int_mapped.GetValues().size(); i++) {
            BOOST_CHECK_EQUAL(int_mapped.GetValues()[i], i + 1);
        }
    }}

    {{
        // More graph data than fits in the target location
        CSeq_graph graph;
        s_InitInterval(graph.SetLoc().SetInt(), 4, 8, 28);
        graph.SetNumval(20);
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 20; i++) {
            int_graph.SetValues().push_back(i);
        }
        BOOST_CHECK_THROW(mapper.Map(graph), CAnnotMapperException);
    }}

    {{
        // Width of gi 999 is unknown
        CSeq_graph graph;
        {{
            CRef<CSeq_interval> loc_int(new CSeq_interval);
            s_InitInterval(*loc_int, 4, 5, 14);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
            loc_int.Reset(new CSeq_interval);
            s_InitInterval(*loc_int, 999, 105, 114);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
            loc_int.Reset(new CSeq_interval);
            s_InitInterval(*loc_int, 4, 15, 24);
            graph.SetLoc().SetPacked_int().Set().push_back(loc_int);
        }}
        graph.SetNumval(30);
        CInt_graph& int_graph = graph.SetGraph().SetInt();
        int_graph.SetMax(100);
        int_graph.SetMin(0);
        int_graph.SetAxis(0);
        for (int i = 0; i < 30; i++) {
            int_graph.SetValues().push_back(i);
        }
        BOOST_CHECK_THROW(mapper.Map(graph), CAnnotMapperException);
    }}
}
