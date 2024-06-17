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
 * Author:  Justin Foley, NCBI
 *
 * File Description:
 *   tests for 5-column feature table reader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
// #define NCBI_BOOST_NO_AUTO_TEST_MAIN

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/readfeat.hpp>
#include <serial/serialutil.hpp>
#include <util/line_reader.hpp>
// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
    void throw_exception(std::exception const& e) { throw e; }
} // namespace boost

#endif

static CConstRef<CSeq_feat>
s_GetSeqFeatFromLineError(const ILineError& lineError)
{
    auto pObject = dynamic_cast<const CObjReaderLineException&>(lineError).GetObject();
    _ASSERT(pObject);
    return ConstRef(CTypeConverter<CSeq_feat>::SafeCast(pObject.GetPointerOrNull()));
}

BOOST_AUTO_TEST_CASE(Test_MessageLogging)
{
    string                  filename = "./5col_feature_table_files/gb-7414.tbl";
    CNcbiIfstream           istream(filename);
    CStreamLineReader       lineReader(istream);
    CMessageListenerLenient listener;
    CFeature_table_reader   tableReader(lineReader, &listener);

    auto pAnnot =
        tableReader.ReadSequinFeatureTable(CFeature_table_reader::fIncludeObjectInMsg);
    BOOST_CHECK(pAnnot);
    pAnnot.Reset(); // Check that referenced features persist after Seq-annot has
                    // been deleted

    BOOST_CHECK_EQUAL(listener.Count(), 3);
    const auto& firstError = listener.GetError(0);
    BOOST_CHECK_EQUAL(firstError.Problem(),
                      ILineError::eProblem_UnrecognizedQualifierName);
    auto pFirstFeat = s_GetSeqFeatFromLineError(firstError);
    BOOST_CHECK(pFirstFeat->IsSetData() && pFirstFeat->GetData().IsCdregion());

    const auto& secondError = listener.GetError(1);
    BOOST_CHECK_EQUAL(secondError.Problem(),
                      ILineError::eProblem_QualifierBadValue);
    auto pSecondFeat = s_GetSeqFeatFromLineError(secondError);
    BOOST_CHECK(pFirstFeat->Equals(*pSecondFeat));

    const auto& thirdError = listener.GetError(2);
    BOOST_CHECK_EQUAL(thirdError.Problem(),
                      ILineError::eProblem_QualifierBadValue);
}
