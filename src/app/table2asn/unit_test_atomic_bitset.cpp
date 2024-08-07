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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   Unit tests for atomic bitsets
*
*/

#include <ncbi_pch.hpp>
#include "atomic_bitset.hpp"

#include <corelib/test_boost.hpp>


BOOST_AUTO_TEST_CASE(RunTests)
{

    ncbi::CAtomicBitSet bits(10000);

    bits.reset(200);
    bits.reset(201);
    bits.set(300);
    bits.set(200);

}
