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
 * Author: Greg Boratyn
 *
 */

/** @file pssm_test_util.hpp
 * Utilities to develop and debug unit tests that deal with PSSM computation
 */

#ifndef _PSSM_TEST_UTIL_HPP
#define _PSSM_TEST_UTIL_HPP

#include <string>

#include <corelib/ncbistd.hpp>
#include <blast_objmgr_priv.hpp>

#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_setup.h>
#include <objmgr/scope.hpp>
#include "blast_psi_priv.h"

using namespace std;
using namespace ncbi;
using namespace ncbi::blast;

/// This class exists merely to call private methods in CPsiBlastInputData
/// and CPssmEngine.  Both clases declare this one as a friend.
class CPssmCreateTestFixture {
public:
   /// Gets error strings from a CPssmEngine private method
   /// @param error_code input integer code
    static string 
   x_ErrorCodeToString(int error_code)
   {
         return CPssmEngine::x_ErrorCodeToString(error_code); 
   }

   /// Gets Subject sequence from a CPsiBlastInputData private method
   /// @param ds alignment input
   /// @param scope allos fetching of sequence
   /// @param sequence_data return value for sequence.
   static void
   x_GetSubjectSequence(const objects::CDense_seg& ds, objects::CScope& scope,
                          string& sequence_data)
   {
         return CPsiBlastInputData::x_GetSubjectSequence(ds, scope, sequence_data);
   }

   /// Accesses CPssmEngine private method.
   /// Copies query sequence and adds protein sentinel bytes at the beginning
   /// and at the end of the sequence.
   /// @param query sequence to copy [in]
   /// @param query_length length of the sequence above [in]
   /// @throws CBlastException if does not have enough memory
   /// @return copy of query guarded by protein sentinel bytes
   static unsigned char*
   x_GuardProteinQuery(const unsigned char* query,
                                 unsigned int query_length)
   {
         return CPssmEngine::x_GuardProteinQuery(query, query_length);
   }

   /// Accesses CPsiBlastInputData private method.  
   /// Returns the number of sequences that make up the multiple sequence
   /// alignment
   /// @param input Instance of CPsiBlastInputData
   static unsigned int 
   GetNumAlignedSequences(const CPsiBlastInputData& input)
   {
         return input.GetNumAlignedSequences();
   }
};

/// @param query protein sequence in ncbistdaa with sentinel bytes
/// @param query_size length of the query sequence (w/o including sentinel
//  bytes)
BlastScoreBlk* InitializeBlastScoreBlk(const unsigned char* query,
                                       Uint4 query_size);

/// template specializations to automate deallocation of internal BLAST
/// structures with ncbi::AutoPtr
BEGIN_NCBI_SCOPE

// FIXME: declare RAII classes for these?
template <>
struct Deleter<_PSIAlignedBlock> {
    static void Delete(_PSIAlignedBlock* p)
    { _PSIAlignedBlockFree(p); }
};

template <>
struct Deleter<_PSISequenceWeights> {
    static void Delete(_PSISequenceWeights* p)
    { _PSISequenceWeightsFree(p); }
};

template <>
struct Deleter<_PSIInternalPssmData> {
    static void Delete(_PSIInternalPssmData* p)
    { _PSIInternalPssmDataFree(p); }
};

template <>
struct Deleter<_PSIMsa> {
    static void Delete(_PSIMsa* p)
    { _PSIMsaFree(p); }
};

template <>
struct Deleter<_PSIPackedMsa> {
    static void Delete(_PSIPackedMsa* p)
    { _PSIPackedMsaFree(p); }
};

END_NCBI_SCOPE




#endif // _PSSM_TEST_UTIL_HPP
