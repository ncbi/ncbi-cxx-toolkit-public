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
* Author:  Vyacheslav Chetvernin
*
* File Description:
*   Score unit test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_serial.hpp>
#include <objtools/alnmgr/sparse_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_builders.hpp>
#include <objtools/alnmgr/sparse_ci.hpp>
#include <objtools/alnmgr/aln_generators.hpp>
#include <objtools/alnmgr/score_builder_base.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);



NCBITEST_INIT_CMDLINE(descrs)
{
    descrs->AddFlag("print_exp", "Print expected values instead of testing");
}


bool DumpExpected(void)
{
    return CNcbiApplication::Instance()->GetArgs()["print_exp"];
}


CScope& GetScope(void)
{
    static CRef<CScope> s_Scope;
    if ( !s_Scope ) {
        CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*objmgr);
        
        s_Scope.Reset(new CScope(*objmgr));
        s_Scope->AddDefaults();
    }
    return *s_Scope;
}


// Helper function for loading alignments. Returns number of alignments loaded
// into the container. The expected format is:
// <int-number-of-alignments>
// <Seq-align>+
size_t LoadAligns(CNcbiIstream& in,
                  CAlnContainer& aligns,
                  size_t limit = 0)
{
    size_t num_aligns = 0;
    while ( !in.eof() ) {
        try {
            CRef<CSeq_align> align(new CSeq_align);
            in >> MSerial_AsnText >> *align;
            align->Validate(true);
            aligns.insert(*align);
            num_aligns++;
            if (limit > 0  &&  num_aligns >= limit) break;
        }
        catch (CIOException e) {
            break;
        }
    }
    return num_aligns;
}


BOOST_AUTO_TEST_CASE(TestAltStartIdentity)
{
    string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id gi 487489526, \
    genomic-id gi 556503834, \
    genomic-strand minus, \
    product-type protein, \
    exons { \
      { \
        product-start protpos { \
          amin 0, \
          frame 1 \
        }, \
        product-end protpos { \
          amin 259, \
          frame 3 \
        }, \
        genomic-start 1642491, \
        genomic-end 1643270, \
        parts { \
          diag 780 \
        } \
      } \
    }, \
    product-length 260 \
  } \
} ";

    CNcbiIstrstream istrs(buf);
    unique_ptr<CObjectIStream> istr(CObjectIStream::Open(eSerial_AsnText, istrs));
    CSeq_align align;
    *istr >> align;
    BOOST_CHECK_NO_THROW(align.Validate(true));

    CScope& scope = GetScope();
    CScoreBuilderBase score_builder;
    int value;
    score_builder.AddScore(scope, align, CSeq_align::eScore_IdentityCount);
    BOOST_CHECK(align.GetNamedScore(CSeq_align::eScore_IdentityCount, value));
    BOOST_CHECK_EQUAL(value, 780);
    score_builder.AddScore(scope, align, CSeq_align::eScore_MismatchCount);
    BOOST_CHECK(align.GetNamedScore(CSeq_align::eScore_MismatchCount, value));
    BOOST_CHECK_EQUAL(value, 0);
    score_builder.AddScore(scope, align, CSeq_align::eScore_PercentIdentity_Gapped);
    int dbl_value;
    BOOST_CHECK(align.GetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped, dbl_value));
    BOOST_CHECK_CLOSE(dbl_value, 100.0, DBL_EPSILON);
}
