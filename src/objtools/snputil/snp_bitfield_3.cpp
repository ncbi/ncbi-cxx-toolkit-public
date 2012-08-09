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
 * Authors:  Melvin Quintos
 *
 * File Description:
 *  Provides implementation of CSnpBitfield3 class. See snp_bitfield_3.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_3.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield3::CSnpBitfield3( const std::vector<char> &rhs )
 : CSnpBitfield2(rhs)
{
}

int CSnpBitfield3::GetVersion() const
{
    return 3;
}

bool CSnpBitfield3::IsTrue(CSnpBitfield::EProperty prop) const
{
    bool ret = false;

    // Special case for 'has reference'
    if (prop == CSnpBitfield::eHasReference) {
        return (m_listBytes[4] & 0x02);
    }

    // Return false if property queried is
    // newer than last property implemented at version 3 release
    // last property implemented was 'eIsContigAlleleAbsent'
    if(prop > CSnpBitfield::eIsContigAlleleAbsent)
        return false;

    if (prop == CSnpBitfield::eIsContigAlleleAbsent)
        ret = (m_listBytes[11] & 0x10) != 0;  // on byte 11, bit 5
    else
        ret = CSnpBitfield2::IsTrue(prop);

    return ret;
}

CSnpBitfield::IEncoding * CSnpBitfield3::Clone()
{
    CSnpBitfield3 * obj = new CSnpBitfield3();

    memcpy(obj->m_listBytes, m_listBytes, sizeof(m_listBytes));
    obj->m_strBits = m_strBits;

    return obj;
}

END_NCBI_SCOPE
