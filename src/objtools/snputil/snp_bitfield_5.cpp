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
 *  Provides implementation of CSnpBitfield5 class. See snp_bitfield_5.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_5.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield5::CSnpBitfield5(const CSeq_feat& feat)
 : CSnpBitfield4(feat)
{
}

int CSnpBitfield5::GetVersion() const
{
    return 5;
}

bool CSnpBitfield5::IsTrue(CSnpBitfield::EProperty prop) const
{
    int ret(0);

    // Return false if property queried is
    // newer than last property implemented at version 5 release
    if(prop > CSnpBitfield::ePropertyV5Last)
        return false;

    switch (prop) {
        // return false for all fields deprecated in V5
        case CSnpBitfield::eTGP2009Pilot_depr:
        case CSnpBitfield::eTGP2010Pilot_depr:
        case CSnpBitfield::eTGP2010Production_depr:
        case CSnpBitfield::ePhase1Genotyped_depr:
        case CSnpBitfield::ePhase2Genotyped_depr:
            ret = false;
            break;
        // proper values for all fields introduced in V5
        case CSnpBitfield::eHasPubmedArticle:
            ret = (m_listBytes[2] & BIT_4); // on byte 2, bit 4
            break;
        case CSnpBitfield::eHasProvisionalTPA:
            ret = (m_listBytes[2] & BIT_5); // on byte 2, bit 5
            break;
        case CSnpBitfield::eIsPrecious:
            ret = (m_listBytes[2] & BIT_6); // on byte 2, bit 6
            break;
        case CSnpBitfield::eIsClinical:
            ret = (m_listBytes[2] & BIT_7); // on byte 2, bit 7
            break;
        case CSnpBitfield::eIsSomatic:
            ret = (m_listBytes[11] & BIT_6); // on byte 11 (F9), bit 6
            break;
        case CSnpBitfield::eIsSuspect:
            ret = (m_listBytes[11] & BIT_7); // on byte 11 (F9), bit 7
            break;
        case CSnpBitfield::eTGPPhase1:
            ret = (m_listBytes[8] & BIT_5); // on byte 8 (F6), bit 5
            break;
        case CSnpBitfield::eTGPPilot:
            ret = (m_listBytes[8] & BIT_4); // on byte 8 (F6), bit 4
            break;
        case CSnpBitfield::eTGPOnly:
            ret = (m_listBytes[8] & (BIT_2 | BIT_3)) == 0x04; // on byte 8 (F6), bit 3 and _not_ bit 2
            break;
        case CSnpBitfield::eTGPNone:
            ret = (m_listBytes[8] & (BIT_2 | BIT_3)) == 0x02; // on byte 8 (F6), bit 2 and _not_ bit 3
            break;
        case CSnpBitfield::eTGPBoth:
            ret = (m_listBytes[8] & (BIT_2 | BIT_3)) == 0x06; // on byte 8 (F6), both bit 2 and bit 3 are set
            break;
        case CSnpBitfield::eTGPOnlyNotExclusive:
            ret = (m_listBytes[8] & BIT_3); // on byte 8 (F6), bit 3
            break;
        case CSnpBitfield::eTGPNoneNotExclusive:
            ret = (m_listBytes[8] & BIT_2); // on byte 8 (F6), bit 2
            break;
        case CSnpBitfield::eGMAF0dot01:
            ret = (m_listBytes[6] & BIT_5); // on byte 6 (F4), bit 5
            break;
        case CSnpBitfield::eTGPPhase3:
            ret = (m_listBytes[8] & BIT_6); // on byte 8 (F6), bit 6
            break;
        case CSnpBitfield::eTGPPhase1NotPhase3:
            ret = (m_listBytes[8] & (BIT_5 | BIT_6)) == 0x10; // on byte 8 (F6), 	bit-5, not bit-6
            break;
        case CSnpBitfield::eTGPPhase3NotPhase1:
            ret = (m_listBytes[8] & (BIT_5 | BIT_6)) == 0x20; // on byte 8 (F6), 	bit-6, not bit-5
            break;
        case CSnpBitfield::eTGPPhase3OrPhase1:
            ret = (m_listBytes[8] & (BIT_5 | BIT_6)); // on byte 8 (F6), 	bit-5 or bit-6
            break;
        case CSnpBitfield::eTGPNeitherPhase3NorPhase1:
            ret = !(m_listBytes[8] & (BIT_5 | BIT_6)); // on byte 8 (F6), 	neither bit-5 nor bit-6
            break;
        case CSnpBitfield::eTGPUniquePhase3:
            ret = (m_listBytes[8] & (BIT_2 | BIT_5 | BIT_6)) == 0x20; // on byte 8 (F6), bit-6 AND (not bit-2) AND (not bit 5)
            break;
        case CSnpBitfield::eTGPNovelInPhase3:
            ret = (m_listBytes[8] & (BIT_2 |  BIT_6)) == 0x20; // on byte 8 (F6), bit-6 AND (not bit-2)
            break;
        case CSnpBitfield::eTGPBit7:
            ret = (m_listBytes[8] & (BIT_7)); // on byte 8 (F6), bit-7
            break;
        // all other values inherited from V4
        default:
            ret = CSnpBitfield4::IsTrue(prop);
            break;
    }

    return (bool) ret;
}

bool CSnpBitfield5::IsTrue( CSnpBitfield::EFunctionClass prop ) const
{
    bool ret = false;

    // looking for a specific function class
    unsigned char byte4 = m_listBytes[4];

    // the 'Has reference' bit may be set.  So turn it off for test.
    byte4 &= 0xfd; // 1111 1101 <- mask to set the 2nd bit to zero

    switch (prop) {
        // using byte4
        case  CSnpBitfield::eStopGain:      ret = (byte4 & fBit2);  break;
        case  CSnpBitfield::eStopLoss:      ret = (byte4 & fBit5);  break;

        default:
            ret = CSnpBitfield2::IsTrue( prop );
    }

    return ret;
}

CSnpBitfield::EFunctionClass CSnpBitfield5::GetFunctionClass() const
{
    // On bytes 3 and 4
    unsigned char byte3 = m_listBytes[3];
    unsigned char byte4 = m_listBytes[4];

    // the 'Has reference' bit may be set.  So turn it off for now.
    byte4 &= 0xfd; // 1111 1101 <- mask to set the 2nd bit to zero

    if (byte3 != 0 && byte4 != 0)
        return CSnpBitfield::eMultipleFxn;

    switch ( byte4 )
    {
    case fBit2:     return  CSnpBitfield::eStopGain;
    case fBit5:     return  CSnpBitfield::eStopLoss;
    }

    return CSnpBitfield4::GetFunctionClass();
}


CSnpBitfield::IEncoding * CSnpBitfield5::Clone()
{
    CSnpBitfield5 * obj = new CSnpBitfield5();

    memcpy(obj->m_listBytes, m_listBytes, sizeof(m_listBytes));
    obj->m_strBits = m_strBits;

    return obj;
}

END_NCBI_SCOPE
