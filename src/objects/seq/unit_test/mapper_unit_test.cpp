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


// Read two seq-locs, initialize seq-loc mapper.
CSeq_loc_Mapper_Base* CreateMapperFromSeq_locs(CNcbiIstream& in)
{
    CSeq_loc src, dst;
    in >> MSerial_AsnText >> src;
    in >> MSerial_AsnText >> dst;
    return new CSeq_loc_Mapper_Base(src, dst);
}


// Map the original seq-loc, read the reference location, compare to the
// mapped one.
void TestMappingSeq_loc(CSeq_loc_Mapper_Base& mapper,
                          const CSeq_loc& orig,
                          CNcbiIstream& in)
{
    CSeq_loc ref_mapped;
    in >> MSerial_AsnText >> ref_mapped;
    CRef<CSeq_loc> mapped = mapper.Map(orig);
    BOOST_CHECK(mapped);
    bool eq = mapped->Equals(ref_mapped);
    BOOST_CHECK(mapped->Equals(ref_mapped));
    if ( !eq ) {
        cout << "Expected mapped location:" << endl;
        cout << MSerial_AsnText << ref_mapped;
        cout << "Real mapped location:" << endl;
        cout << MSerial_AsnText << *mapped;
    }
}


void TestMappingSeq_loc(CSeq_loc_Mapper_Base& mapper,
                        CNcbiIstream& in)
{
    CSeq_loc orig;
    in >> MSerial_AsnText >> orig;
    TestMappingSeq_loc(mapper, orig, in);
}


void TestMappingSeq_loc_Exception(CSeq_loc_Mapper_Base& mapper,
                                  CNcbiIstream& in)
{
    CSeq_loc orig;
    in >> MSerial_AsnText >> orig;
    BOOST_CHECK_THROW(mapper.Map(orig), CAnnotMapperException);
}


// Map the original seq-align, read the reference alignment, compare to the
// mapped one.
void TestMappingSeq_align(CSeq_loc_Mapper_Base& mapper,
                          const CSeq_align& orig,
                          CNcbiIstream& in)
{
    CSeq_align ref_mapped;
    in >> MSerial_AsnText >> ref_mapped;
    CRef<CSeq_align> mapped = mapper.Map(orig);
    BOOST_CHECK(mapped);
    bool eq = mapped->Equals(ref_mapped);
    BOOST_CHECK(mapped->Equals(ref_mapped));
    if ( !eq ) {
        cout << "Expected mapped alignment:" << endl;
        cout << MSerial_AsnText << ref_mapped;
        cout << "Real mapped alignment:" << endl;
        cout << MSerial_AsnText << *mapped;
    }
}


void TestMappingSeq_align(CSeq_loc_Mapper_Base& mapper,
                          CNcbiIstream& in)
{
    CSeq_align orig;
    in >> MSerial_AsnText >> orig;
    TestMappingSeq_align(mapper, orig, in);
}


void TestMappingSeq_align_Exception(CSeq_loc_Mapper_Base& mapper,
                                    CNcbiIstream& in)
{
    CSeq_align orig;
    in >> MSerial_AsnText >> orig;
    BOOST_CHECK_THROW(mapper.Map(orig), CAnnotMapperException);
}


// Map the original seq-graph, read the reference graph, compare to the
// mapped one.
void TestMappingSeq_graph(CSeq_loc_Mapper_Base& mapper,
                          const CSeq_graph& orig,
                          CNcbiIstream& in)
{
    CSeq_graph ref_mapped;
    in >> MSerial_AsnText >> ref_mapped;
    CRef<CSeq_graph> mapped = mapper.Map(orig);
    BOOST_CHECK(mapped);
    bool eq = mapped->Equals(ref_mapped);
    BOOST_CHECK(mapped->Equals(ref_mapped));
    if ( !eq ) {
        cout << "Expected mapped graph:" << endl;
        cout << MSerial_AsnText << ref_mapped;
        cout << "Real mapped graph:" << endl;
        cout << MSerial_AsnText << *mapped;
    }
}


void TestMappingSeq_graph(CSeq_loc_Mapper_Base& mapper,
                          CNcbiIstream& in)
{
    CSeq_graph orig;
    in >> MSerial_AsnText >> orig;
    TestMappingSeq_graph(mapper, orig, in);
}


void TestMappingSeq_graph_Exception(CSeq_loc_Mapper_Base& mapper,
                                    CNcbiIstream& in)
{
    CSeq_graph orig;
    in >> MSerial_AsnText >> orig;
    BOOST_CHECK_THROW(mapper.Map(orig), CAnnotMapperException);
}


void TestMapping_Simple(CNcbiIstream& in)
{
    cout << "Basic mapping and truncaction test" << endl;

    CSeq_loc src, dst_plus, dst_minus;
    in >> MSerial_AsnText >> src;
    in >> MSerial_AsnText >> dst_plus;
    in >> MSerial_AsnText >> dst_minus;
    CSeq_loc_Mapper_Base mapper_plus(src, dst_plus);
    CSeq_loc_Mapper_Base mapper_minus(src, dst_minus);

    CSeq_loc orig;

    in >> MSerial_AsnText >> orig;
    cout << "  Simple interval" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Simple interval, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Truncated on the right" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Truncated on the right, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Truncated on the left" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Truncated on the left, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Truncated on both ends" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Truncated on both ends, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Minus strand interval" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand interval, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Minus strand interval, truncated on the right" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand interval, truncated on the right, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Minus strand interval, truncated on the left" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand interval, truncated on the left, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Minus strand interval, truncated on both ends" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand interval, truncated on both ends, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    cout << "  Null seq-loc" << endl;
    TestMappingSeq_loc(mapper_plus, in);

    cout << "  Empty seq-loc" << endl;
    TestMappingSeq_loc(mapper_plus, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Whole seq-loc" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Whole seq-loc, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Point" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Point, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Packed-points" << endl;
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Packed-points, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, orig, in);

    cout << "  Bond" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Bond, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
}


void TestMapping_Order(CNcbiIstream& in)
{
    cout << "Order of mapped intervals" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Mapping plus to plus strand" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping minus to minus strand" << endl;
    TestMappingSeq_loc(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping plus to minus strand (src on plus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping minus to plus strand (src on plus)" << endl;
    TestMappingSeq_loc(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping plus to minus strand (src on minus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping minus to plus strand (src on minus)" << endl;
    TestMappingSeq_loc(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping through a mix, plus to plus strand" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, plus to plus strand, with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();
    cout << "  Mapping through a mix, minus to minus strand" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, minus to minus strand, with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping through a mix, plus to minus strand (src on plus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, plus to minus (src on plus), with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();
    cout << "  Mapping through a mix, minus to plus strand (src on plus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, minus to plus (src on plus), with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping through a mix, plus to minus strand (src on minus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, plus to minus strand (src on minus), with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();
    cout << "  Mapping through a mix, minus to plus strand (src on minus)" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping through a mix, minus to plus strand (src on minus), with merge" << endl;
    mapper->SetMergeAbutting();
    TestMappingSeq_loc(*mapper, in);
    mapper->SetMergeNone();
}


void TestMapping_Merging(CNcbiIstream& in)
{
    cout << "Merging of mapped intervals" << endl;
    CSeq_loc src, dst_plus, dst_minus;
    in >> MSerial_AsnText >> src;
    in >> MSerial_AsnText >> dst_plus;
    in >> MSerial_AsnText >> dst_minus;
    CSeq_loc_Mapper_Base mapper_plus(src, dst_plus);
    CSeq_loc_Mapper_Base mapper_minus(src, dst_minus);

    CSeq_loc orig;
    in >> MSerial_AsnText >> orig;

    cout << "  No merging" << endl;
    mapper_plus.SetMergeNone();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  No merging, reverse strand mapping" << endl;
    mapper_minus.SetMergeNone();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Merge abutting" << endl;
    mapper_plus.SetMergeAbutting();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Merge abutting, reverse strand mapping" << endl;
    mapper_minus.SetMergeAbutting();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Merge contained" << endl;
    mapper_plus.SetMergeContained();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Merge contained, reverse strand mapping" << endl;
    mapper_minus.SetMergeContained();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Merge all" << endl;
    mapper_plus.SetMergeAll();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Merge all, reverse strand mapping" << endl;
    mapper_minus.SetMergeAll();
    TestMappingSeq_loc(mapper_minus, orig, in);

    in >> MSerial_AsnText >> orig;

    cout << "  Minus strand source, no merging" << endl;
    mapper_plus.SetMergeNone();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand source, no merging, reverse strand mapping" << endl;
    mapper_minus.SetMergeNone();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Minus strand source, merge abutting" << endl;
    mapper_plus.SetMergeAbutting();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand source, merge abutting, reverse strand mapping" << endl;
    mapper_minus.SetMergeAbutting();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Minus strand source, merge contained" << endl;
    mapper_plus.SetMergeContained();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand source, merge contained, reverse strand mapping" << endl;
    mapper_minus.SetMergeContained();
    TestMappingSeq_loc(mapper_minus, orig, in);
    cout << "  Minus strand source, merge all" << endl;
    mapper_plus.SetMergeAll();
    TestMappingSeq_loc(mapper_plus, orig, in);
    cout << "  Minus strand source, merge all, reverse strand mapping" << endl;
    mapper_minus.SetMergeAll();
    TestMappingSeq_loc(mapper_minus, orig, in);
}


void TestMapping_ProtToNuc(CNcbiIstream& in)
{
    // Incomplete, needs to be updated
    cout << "Mapping from protein to nucleotide" << endl;
    CSeq_loc src, dst_plus, dst_minus;
    in >> MSerial_AsnText >> src;
    in >> MSerial_AsnText >> dst_plus;
    in >> MSerial_AsnText >> dst_minus;
    CSeq_loc_Mapper_Base mapper_plus(src, dst_plus);
    CSeq_loc_Mapper_Base mapper_minus(src, dst_minus);

    cout << "  Simple interval" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Partial on the right" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Original location on minus strand" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Original location on minus strand, partial" << endl;
    TestMappingSeq_loc(mapper_plus, in);

    cout << "  Simple interval, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Partial on the right, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Original location on minus strand, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Original location on minus strand, partial, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
}


void TestMapping_NucToProt(CNcbiIstream& in)
{
    // Incomplete, needs to be updated
    cout << "Mapping from nucleotide to protein" << endl;
    CSeq_loc src_plus, src_minus, dst;
    in >> MSerial_AsnText >> src_plus;
    in >> MSerial_AsnText >> src_minus;
    in >> MSerial_AsnText >> dst;
    CSeq_loc_Mapper_Base mapper_plus(src_plus, dst);
    CSeq_loc_Mapper_Base mapper_minus(src_minus, dst);

    cout << "  Simple interval" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Partial on the right" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Original location on minus strand" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Original location on minus strand, partial" << endl;
    TestMappingSeq_loc(mapper_plus, in);
    cout << "  Shifted nucleotide positions (incomplete codons)" << endl;
    TestMappingSeq_loc(mapper_plus, in);

    cout << "  Simple interval, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Partial on the right, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Original location on minus strand, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Original location on minus strand, partial, reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
    cout << "  Shifted nucleotide positions (incomplete codons), reversed strand" << endl;
    TestMappingSeq_loc(mapper_minus, in);
}


void TestMapping_ThroughMix(CNcbiIstream& in)
{
    cout << "Mapping through mix" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    mapper->SetMergeAbutting();
    cout << "  Single interval overlapping all source ranges" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Single interval on minus strand, partial overlapping" << endl;
    TestMappingSeq_loc(*mapper, in);

    cout << "Mapping through mix, reversed strand" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Original sec-loc is the same as mapping source" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping a packed-int" << endl;
    TestMappingSeq_loc(*mapper, in);
    cout << "  Mapping a multi-level seq-loc" << endl;
    TestMappingSeq_loc(*mapper, in);
}


void TestMapping_Dendiag(CNcbiIstream& in)
{
    cout << "Mapping dense-diag alignment" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Single segment" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Unsupported mapped alignment - gaps in dense-diag" << endl;
    TestMappingSeq_align_Exception(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Single segment, reversed strand" << endl;
    TestMappingSeq_align(*mapper, in);
}


void TestMapping_Denseg(CNcbiIstream& in)
{
    cout << "Mapping dense-seg alignments" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Nuc to prot, converted to std-seg (mixed types)" << endl;
    TestMappingSeq_align(*mapper, in);

    cout << "  Unsupported alignment - dense-seg with mixed types" << endl;
    TestMappingSeq_align_Exception(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Setting correct strands in gaps" << endl;
    TestMappingSeq_align(*mapper, in);
}


void TestMapping_Spliced(CNcbiIstream& in)
{
    cout << "Mapping spliced-seg alignments" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Mapping spliced-seg product, nuc to nuc" << endl;
    TestMappingSeq_align(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping spliced-seg product, nuc to prot" << endl;
    TestMappingSeq_align(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping spliced-seg product, nuc to prot, reversed strand" << endl;
    TestMappingSeq_align(*mapper, in);

    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Mapping spliced-seg through multiple ranges" << endl;
    TestMappingSeq_align(*mapper, in);
}


void TestMapping_Scores(CNcbiIstream& in)
{
    cout << "Mapping scores" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Dense-diag - scores are preserved" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Dense-seg, scores are preserved" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Dense-seg - scores are dropped (global and one segment)" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Std-seg, scores are preserved" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Std-seg - scores are dropped (global and one segment)" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Packed-seg, scores are preserved" << endl;
    TestMappingSeq_align(*mapper, in);
    cout << "  Packed-seg - scores are dropped (global and one segment)" << endl;
    TestMappingSeq_align(*mapper, in);
}


void TestMapping_Graph(CNcbiIstream& in)
{
    cout << "Mapping graphs" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    cout << "  Mapping whole graph" << endl;
    TestMappingSeq_graph(*mapper, in);
    cout << "  Partial - skip a range in the middle" << endl;
    TestMappingSeq_graph(*mapper, in);
    cout << "  Mapping a graph on minus strand" << endl;
    TestMappingSeq_graph(*mapper, in);

    cout << "Graph mapping, nuc to prot" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    cout << "  Simple graph, using comp=3 to allow mapping" << endl;
    TestMappingSeq_graph(*mapper, in);

    cout << "  Unsupported: different original and mapped location lengths" << endl;
    TestMappingSeq_graph_Exception(*mapper, in);
    cout << "  Unsupported: unknown destination sequence type" << endl;
    TestMappingSeq_graph_Exception(*mapper, in);
}


void TestMapping_AlignmentsToParts(CNcbiIstream& in)
{
    cout << "Test mapping alignments to bioseq segments" << endl;
    CSeq_align orig;

    // Although mapping to bioseq segments, we don't have an Object Manager
    // here. Using seq-locs instead.

    in >> MSerial_AsnText >> orig;
    cout << "  Alignment #1, mapping row 1" << endl;
    auto_ptr<CSeq_loc_Mapper_Base> mapper(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);
    cout << "  Alignment #1, mapping row 2" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Alignment #2, mapping row 1" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);
    cout << "  Alignment #2, mapping row 2" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Alignment #3, mapping row 1" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);
    cout << "  Alignment #3, mapping row 2" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);

    in >> MSerial_AsnText >> orig;
    cout << "  Alignment #4, mapping row 1" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);
    cout << "  Alignment #4, mapping row 2" << endl;
    mapper.reset(CreateMapperFromSeq_locs(in));
    TestMappingSeq_align(*mapper, orig, in);
}


void TestMapping_ThroughAlignments(CNcbiIstream& in)
{
    cout << "Test mapping through alignments" << endl;
    CSeq_align aln;

    const char* titles[] = {
        "  Mapping through dense-diag",
        "  Mapping through dense-seg (with some gaps)",
        "  Mapping through packed-seg (with some gaps)",
        "  Mapping through std-seg",
        "  Mapping through disc",
        "  Mapping through spliced-seg",
        "  Mapping through sparse-seg"
    };

    for (int i = 0; i < sizeof(titles)/sizeof(titles[0]); i++) {
        cout << titles[i] << endl;
        in >> MSerial_AsnText >> aln;
        auto_ptr<CSeq_loc_Mapper_Base> mapper(new CSeq_loc_Mapper_Base(aln, 0));
        cout << "    Whole sequence" << endl;
        TestMappingSeq_loc(*mapper, in);
        cout << "    Interval, complete" << endl;
        TestMappingSeq_loc(*mapper, in);
        cout << "    Interval, split" << endl;
        TestMappingSeq_loc(*mapper, in);
    }
}


BOOST_AUTO_TEST_CASE(s_TestMapping)
{
    CNcbiIfstream in("mapper_unit_test.asn");
    TestMapping_Simple(in);
    TestMapping_Order(in);
    TestMapping_Merging(in);
    TestMapping_ProtToNuc(in);
    TestMapping_NucToProt(in);
    TestMapping_ThroughMix(in);
    TestMapping_Dendiag(in);
    TestMapping_Denseg(in);
    TestMapping_Spliced(in);
    TestMapping_Scores(in);
    TestMapping_Graph(in);
    TestMapping_AlignmentsToParts(in);
    TestMapping_ThroughAlignments(in);
}
