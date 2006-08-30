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
 * Authors:  Jason Papadopoulos
 *
 * File Description:
 *   Unit tests for CBlastInput, CBlastInputSource and derived classes
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#include <boost/current_function.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using boost::unit_test::test_suite;

// Use macros rather than inline functions to get accurate line number reports

#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_ERROR("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_ERROR("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))
#define CHECK_THROW(s, x) BOOST_CHECK_THROW(s, x)

#define DECLARE_SOURCE(file)                                    \
    CRef<CObjectManager> om(CObjectManager::GetInstance());     \
    CNcbiIfstream infile(file);                                 \
    CBlastFastaInputSource source(*om, infile);

BOOST_AUTO_UNIT_TEST(s_ReadFastaProtein)
{
    DECLARE_SOURCE("data/aa.129295");

    CHECK(source.End() == false);
    blast::SSeqLoc ssl = source.GetNextSSeqLoc();
    CHECK(source.End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    CHECK_EQUAL((TSeqPos)231, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CHECK(!ssl.mask);
}

BOOST_AUTO_UNIT_TEST(s_RangeBoth)
{
    DECLARE_SOURCE("data/aa.129295");

    source.m_Config.SetFrom(50);
    source.m_Config.SetTo(100);
    blast::SSeqLoc ssl = source.GetNextSSeqLoc();

    CHECK_EQUAL((TSeqPos)50, ssl.seqloc->GetInt().GetFrom());
    CHECK_EQUAL((TSeqPos)100, ssl.seqloc->GetInt().GetTo());
}

BOOST_AUTO_UNIT_TEST(s_RangeStartOnly)
{
    DECLARE_SOURCE("data/aa.129295");

    source.m_Config.SetFrom(50);
    blast::SSeqLoc ssl = source.GetNextSSeqLoc();

    CHECK_EQUAL((TSeqPos)50, ssl.seqloc->GetInt().GetFrom());
    CHECK_EQUAL((TSeqPos)231, ssl.seqloc->GetInt().GetTo());
}

BOOST_AUTO_UNIT_TEST(s_RangeInvalid)
{
    DECLARE_SOURCE("data/aa.129295");

    source.m_Config.SetFrom(100);
    source.m_Config.SetTo(50);
    CHECK_THROW(source.GetNextSSeqLoc(), CObjReaderException);
}

BOOST_AUTO_UNIT_TEST(s_ParseDefline)
{
    DECLARE_SOURCE("data/aa.129295");

    source.m_Config.SetBelieveDeflines(true);
    blast::SSeqLoc ssl = source.GetNextSSeqLoc();
    CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
    CHECK_EQUAL(129295, ssl.seqloc->GetInt().GetId().GetGi());
}

BOOST_AUTO_UNIT_TEST(s_BadProtStrand)
{
    DECLARE_SOURCE("data/aa.129295");

    source.m_Config.SetStrand(eNa_strand_both);
    CHECK_THROW(source.GetNextSSeqLoc(), CObjReaderException);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/08/30 18:03:40  papadopo
 * add protein fasta parsing unit tests
 *
 * Revision 1.2  2006/08/29 20:43:28  papadopo
 * add includes, add first test
 *
 * Revision 1.1  2006/08/29 20:13:49  papadopo
 * unit tests for blastinput library
 *
 * ===========================================================================
 */

