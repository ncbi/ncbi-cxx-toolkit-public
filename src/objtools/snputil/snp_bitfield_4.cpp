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
 *  Provides implementation of CSnpBitfield4 class. See snp_bitfield_4.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_4.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield4::CSnpBitfield4(const CSeq_feat& feat)
 : CSnpBitfield3(feat)
{
}

int CSnpBitfield4::GetVersion() const
{
    return 4;
}

bool CSnpBitfield4::IsTrue(CSnpBitfield::EProperty prop) const
{
    bool ret = false;

    // Return false if property queried is
    // newer than last property implemented at version 4 release
    // last property implemented was 'eIsValidated'
    if(prop > CSnpBitfield::ePropertyV4Last)
        return false;

    if (prop == CSnpBitfield::eIsDoubleHit_depr)  // eIsDoubleHit was removed
        ret = false;
    else if (prop == CSnpBitfield::eIsValidated)
        ret = (m_listBytes[6] & 0x04) != 0;  // on byte 6, bit 3
    else
        ret = CSnpBitfield3::IsTrue(prop);

    return ret;
}

CSnpBitfield::IEncoding * CSnpBitfield4::Clone()
{
    CSnpBitfield4 * obj = new CSnpBitfield4();

    memcpy(obj->m_listBytes, m_listBytes, sizeof(m_listBytes));
    obj->m_strBits = m_strBits;

    return obj;
}

END_NCBI_SCOPE
