#ifndef RANDOM_GEN__HPP
#define RANDOM_GEN__HPP

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
 * Author:  Clifford Clausen, Denis Vakatov, Jim Ostel, Jonathan Kans,
 *          Greg Schuler
 * Contact: Clifford Clausen
 *
 * File Description:
 *   CRandom implements a lagged Fibonacci (LFG) random number generator (RNG)
 *   with lags 33 and 13, modulus 2^31, and operation '+'. It is a slightly
 *   modified version of Nlm_Random() found in the NCBI C toolkit. It 
 *   generates uniform random numbers between 0 and 2^31 - 1.
 *
 *   More details and literature refs are provided in "random_gen.cpp".
 *
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/07/03 18:36:07  clausen
 * Initial check in
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CRandom::
//

class CRandom
{
public:
    // Constructors
    CRandom(void);
    CRandom(Uint4 seed);

    // Initialize and Seed the random number generator
    void  SetSeed(Uint4 seed);
    Uint4 GetSeed(void);

    // Reset random number generator to initial startup condition
    void Reset(void);

    // Get the next random number
    Uint4 GetRand(void);

private:
    // Static array used to initialize "m_State", and its size
    static const Uint4  sm_State[33];
    static const size_t kStateSize;

    // Instance members
    Uint4  m_State[sizeof(sm_State) / sizeof(sm_State[0])];
    Uint4* m_RJ;
    Uint4* m_RK;
    Uint4  m_Seed;
};




/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CRandom::
//

inline Uint4 CRandom::GetSeed(void)
{
    return m_Seed;
}


inline Uint4 CRandom::GetRand(void)
{
    register Uint4 r;

    r = *m_RK;
    r += *(m_RJ--);
    *(m_RK--) = r;

    if (m_RK < m_State) {
        m_RK = &m_State[sizeof(m_State) / sizeof(m_State[0]) - 1];
    }
    else if (m_RJ < m_State) {
        m_RJ = &m_State[sizeof(m_State) / sizeof(m_State[0]) - 1];
    }

    return (r >> 1) & 0x7fffffff;  // discard the least-random bit
}


END_NCBI_SCOPE

#endif  /* RANDOM_GEN__HPP */
