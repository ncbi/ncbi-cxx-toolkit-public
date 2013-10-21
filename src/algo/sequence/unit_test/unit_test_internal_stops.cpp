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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algo/sequence/internal_stops.hpp>

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

BOOST_AUTO_TEST_CASE(TestProtein)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope scope(*om);
    scope.AddDefaults();

string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id gi 148225248, \
    genomic-id gi 224514980, \
    genomic-strand plus, \
    product-type protein, \
    exons { \
      { \
        product-start protpos { \
          amin 22, \
          frame 1 \
        }, \
        product-end protpos { \
          amin 277, \
          frame 3 \
        }, \
        genomic-start 30641728, \
        genomic-end 30642468, \
        parts { \
          diag 69, \
          product-ins 3, \
          diag 30, \
          product-ins 4, \
          diag 494, \
          product-ins 2, \
          diag 85, \
          product-ins 18, \
          diag 63 \
        }, \
        partial TRUE \
      } \
    }, \
    product-length 278, \
    modifiers { \
      stop-codon-found TRUE \
    } \
  } \
} \
";

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

    CSeq_align align;
    *istr >> align;

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CInternalStopFinder int_stop_finder(scope);

    set<TSeqPos> stops = int_stop_finder.FindStops(align);

    BOOST_CHECK_EQUAL( stops.size(), 2U );
}

BOOST_AUTO_TEST_CASE(TestMRNA)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope scope(*om);
    scope.AddDefaults();

string buf = " \
Seq-align ::= { \
  type disc, \
  dim 2, \
  segs spliced { \
    product-id gi 178405, \
    genomic-id gi 224514917, \
    product-strand plus, \
    genomic-strand minus, \
    product-type transcript, \
    exons { \
      { \
        product-start nucpos 0, \
        product-end nucpos 2083, \
        genomic-start 78159147, \
        genomic-end 78161233, \
        parts { \
          match 159, \
          genomic-ins 1, \
          match 25, \
          genomic-ins 1, \
          match 47, \
          product-ins 1, \
          match 361, \
          genomic-ins 2, \
          match 1491 \
        } \
      } \
    }, \
    product-length 2084 \
  } \
} \
";

    CNcbiIstrstream istrs(buf.c_str());

    CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

    CSeq_align align;
    *istr >> align;

    BOOST_CHECK_NO_THROW(align.Validate(true));

    CInternalStopFinder int_stop_finder(scope);

    set<TSeqPos> stops = int_stop_finder.FindStops(align);

    BOOST_CHECK_EQUAL( stops.size(), 9U );
}

