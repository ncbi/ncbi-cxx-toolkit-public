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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   Implementations of the CDiscreteDistribution class.
 */

#include <ncbi_pch.hpp>

#include <util/distribution.hpp>
#include <util/random_gen.hpp>

BEGIN_NCBI_SCOPE

unsigned CDiscreteDistributionImpl::GetNextValue() const
{
    CRandom::TValue random_number = m_RandomGen->GetRand();

    CRangeListImpl::TRangeVector::const_iterator random_range =
        m_RangeVector.begin() + (random_number % m_RangeVector.size());

    int diff = random_range->second - random_range->first;

    return diff <= 0 ? random_range->first :
            random_range->first + (random_number % (diff + 1));
}

END_NCBI_SCOPE
