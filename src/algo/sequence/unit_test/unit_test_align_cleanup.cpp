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
 * Author: Vyacheslav Chetvernin
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
// #include <corelib/ncbiargs.hpp>
// #include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
// #include <objmgr/scope.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algo/sequence/align_cleanup.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

}

NCBITEST_AUTO_INIT()
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
}

BOOST_AUTO_TEST_CASE(Test1)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope scope(*om);
    scope.AddDefaults();

string buf = " \
Seq-align ::= { \
  type partial, \
  dim 2, \
  score { \
    { \
      id str \"score\", \
      value int 516 \
    }, \
    { \
      id str \"num_ident\",     \
      value int 97            \
    },                        \
    {                         \
      id str \"num_positives\", \
      value int 142           \
    },                        \
    { \
      id str \"pct_identity_gap\", \
      value real { 449074074074074, 10, -13 } \
    } \
  }, \
  segs std { \
    { \
      dim 2, \
      ids { \
        gi 489996512, \
        gi 407681635 \
      }, \
      loc { \
        int { \
          from 2, \
          to 217, \
          strand unknown, \
          id gi 489996512 \
        }, \
        int { \
          from 207988, \
          to 208635, \
          strand minus, \
          id gi 407681635 \
} } } } } \
Seq-align ::= { \
  type partial, \
  dim 2, \
  score { \
    { \
      id str \"score\", \
      value int 73 \
    }, \
    { \
      id str \"num_ident\", \
      value int 16 \
    }, \
    { \
      id str \"num_positives\", \
      value int 21 \
    }, \
    { \
      id str \"pct_identity_gap\", \
      value real { 5, 10, 1 } \
    } \
  }, \
  segs std { \
    { \
      dim 2, \
      ids { \
        gi 489996512, \
        gi 407681635 \
      }, \
      loc { \
        int { \
          from 252, \
          to 283, \
          strand unknown, \
          id gi 489996512 \
        }, \
        int { \
          from 173613, \
          to 173708, \
          strand minus, \
          id gi 407681635 \
} } } } } \
";

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

    CAlignCleanup::TAligns orig_aligns;
    while (!istr->EndOfData()) {
        CRef<CSeq_align> align(new CSeq_align);
        *istr >> *align;
        orig_aligns.push_back(align);
    }

    CAlignCleanup cln(scope);
    cln.SortInputsByScore(true);
    CAlignCleanup::TAligns cleaned_aligns;
    cln.Cleanup(orig_aligns, cleaned_aligns, CAlignCleanup::eAlignVec);

    ITERATE (CAlignCleanup::TAligns, align, cleaned_aligns) {
        cerr << MSerial_AsnText << **align;
    }
}


