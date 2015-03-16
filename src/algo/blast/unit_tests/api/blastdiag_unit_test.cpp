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
* Author:  Tom Madden
*
* File Description:
*   Unit test for BLAST_DiagTable
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/core/blast_extend.h>

#include "test_objmgr.hpp"

using namespace ncbi;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(BlastDiag)

BOOST_AUTO_TEST_CASE(testDiagClear) {
     const Uint4 kQlen=100;
     const Int4 kWindowSize=20;
     const Uint4 kFakeReset = 1; // arbitrary non-zero number to init reset
     const Int4 kFakeLastHit = 40; // arbitrary non-zero number to init last_hit
     const int kFakeSequenceLength = 100; // arbitrary sequence length
     BlastInitialWordOptions word_options;
     BlastInitialWordParameters word_params;
     Blast_ExtendWord *ewp;

     word_options.window_size = kWindowSize;
     word_params.options = &word_options;
     word_params.container_type = eDiagArray;
     BOOST_REQUIRE_EQUAL(0, (Int4)BlastExtendWordNew(kQlen, &word_params, &ewp));
     BLAST_DiagTable* diag_table = ewp->diag_table;

     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         (diag_table->hit_level_array)[i].flag = kFakeReset;
         (diag_table->hit_level_array)[i].last_hit = kFakeLastHit;
     }
     diag_table->offset = INT4_MAX/4; // a large fake number, should be reset to kWindowSize.

     Blast_ExtendWordExit(ewp, kFakeSequenceLength);

     BOOST_REQUIRE_EQUAL(kWindowSize, diag_table->offset);
     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         BOOST_REQUIRE_EQUAL((Uint4)0, (diag_table->hit_level_array)[i].flag);
         BOOST_REQUIRE_EQUAL(-kWindowSize, (diag_table->hit_level_array)[i].last_hit);
     }
   
     ewp = BlastExtendWordFree(ewp);
     BOOST_REQUIRE(ewp == NULL);
  }

// This test is for a full diag that does get reset.
BOOST_AUTO_TEST_CASE(testDiagUpdateFull) {
     const Uint4 kQlen=100;
     const Int4 kWindowSize=20;
     const Uint4 kFakeReset = 1; // arbitrary non-zero number to init reset
     const Int4 kFakeLastHit = 40; // arbitrary non-zero number to init last_hit
     const int kFakeSequenceLength = 100; // arbitrary sequence length
     BlastInitialWordOptions word_options;
     BlastInitialWordParameters word_params;
     Blast_ExtendWord *ewp;

     word_options.window_size = kWindowSize;
     word_params.options = &word_options;
     word_params.container_type = eDiagArray;
     BOOST_REQUIRE_EQUAL(0, (Int4)BlastExtendWordNew(kQlen, &word_params, &ewp));
     BLAST_DiagTable* diag_table = ewp->diag_table;

     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         (diag_table->hit_level_array)[i].flag = kFakeReset;
         (diag_table->hit_level_array)[i].last_hit = kFakeLastHit;
     }
     diag_table->offset = INT4_MAX/4 + 1000; // a large fake number, should be reset to kWindowSize.

     Blast_ExtendWordExit(ewp, kFakeSequenceLength);  

     BOOST_REQUIRE_EQUAL(kWindowSize, diag_table->offset);
     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         BOOST_REQUIRE_EQUAL((Uint4)0, (diag_table->hit_level_array)[i].flag);
         BOOST_REQUIRE_EQUAL(-kWindowSize, (diag_table->hit_level_array)[i].last_hit);
     }
   
     ewp = BlastExtendWordFree(ewp);
     BOOST_REQUIRE(ewp == NULL);
  }

// This test is for a not full diag that does not get reset.
BOOST_AUTO_TEST_CASE(testDiagUpdateNotFull) {
     const Uint4 kQlen=100;
     const Int4 kWindowSize=20;
     const Uint4 kFakeReset = 1; // arbitrary non-zero number to init reset
     const Int4 kFakeLastHit = 40; // arbitrary non-zero number to init last_hit
     const int kFakeSequenceLength = 100; // arbitrary sequence length
     const int kFakeOffset = 100; // arbitrary small number for test.
     BlastInitialWordOptions word_options;
     BlastInitialWordParameters word_params;
     Blast_ExtendWord *ewp;

     word_options.window_size = kWindowSize;
     word_params.options = &word_options;
     word_params.container_type = eDiagArray;
     BOOST_REQUIRE_EQUAL(0, (Int4)BlastExtendWordNew(kQlen, &word_params, &ewp));
     BLAST_DiagTable* diag_table = ewp->diag_table;

     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         (diag_table->hit_level_array)[i].flag = kFakeReset;
         (diag_table->hit_level_array)[i].last_hit = kFakeLastHit;
     }
     diag_table->offset = kFakeOffset;

     Blast_ExtendWordExit(ewp, kFakeSequenceLength);  

     BOOST_REQUIRE_EQUAL((kFakeOffset+kFakeSequenceLength+kWindowSize), diag_table->offset);
     for (int i = 0; i < diag_table->diag_array_length; i++)
     {
         BOOST_REQUIRE_EQUAL(kFakeReset, (diag_table->hit_level_array)[i].flag);
         BOOST_REQUIRE_EQUAL(kFakeLastHit, (diag_table->hit_level_array)[i].last_hit);
     }
   
     ewp = BlastExtendWordFree(ewp);
     BOOST_REQUIRE(ewp == NULL);
  }

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: blastdiag-cppunit.cpp,v $
* Revision 1.7  2006/11/21 17:45:30  papadopo
* change tests to use ExtendWord sructures only, since BlastDiag structures are local to the engine
*
* Revision 1.6  2006/09/27 13:45:23  coulouri
* Use Blast_ExtendWordExit() instead of BlastDiagUpdate()
*
* Revision 1.5  2006/07/27 16:24:40  coulouri
* remove blast_extend_priv.h
*
* Revision 1.4  2006/07/27 15:20:10  coulouri
* rename bitfield
*
* Revision 1.3  2006/07/24 16:52:40  coulouri
* optimize DiagStruct to reduce size of working set
*
* Revision 1.2  2005/03/04 17:20:44  bealer
* - Command line option support.
*
* Revision 1.1  2004/12/17 13:27:55  madden
* Test for BlastDiagClear/Update
*
*
* ===========================================================================
*/
