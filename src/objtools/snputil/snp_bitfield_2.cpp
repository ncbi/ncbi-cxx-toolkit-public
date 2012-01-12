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
 *  Provides implementation of CSnpBitfield2 class. See snp_bitfield_2.hpp
 *  for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_2.hpp"

#include <stdio.h>



BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
// Static globals, typedefs, etc
///////////////////////////////////////////////////////////////////////////////
static const unsigned int NUM_BYTES = 12;

// Create table lookups for the 41 property fields
// table lookup for the byte offset
static const int g_byteOffset[] = {
    1,1,1,1,1,1,1,1,    // F1 Link (first 8 properties)     on byte 1
    2,2,                // F1 Link (next 2 properties)      on byte 2 (excludes new eShortReadArchive property)
    5,5,5,              // F3 Map (next 3 properties)       on byte 5
    6,6,6,6,            // F4 Freq (next 4 properties)      on byte 6
    7,7,7,              // F5 GTY  (next 3 properties)      on byte 7
    -1,8,-1,8,-1,8,     // F6 Hapmap (next 6 properties)    on byte 8 (Note that ePhase[1-3]Attempted are not mapped)
    9,9,9,9,9,9,9,9,    // F7 Phenotype (next 8 properties) on byte 9
    11,11,-1,-1,11,11,  // F9 Quality (next 4 properties)   on byte 11 (excludes Mendel & HW deviation)
    2                   // F1 Link for eShortReadArchive    on byte 2
};

// table lookup of bit offset
static const int g_bitOffset[] = {
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1, CSnpBitfield::IEncoding::fBit2, CSnpBitfield::IEncoding::fBit3, CSnpBitfield::IEncoding::fBit4, CSnpBitfield::IEncoding::fBit5, CSnpBitfield::IEncoding::fBit6, CSnpBitfield::IEncoding::fBit7,
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1,
    CSnpBitfield::IEncoding::fBit2, CSnpBitfield::IEncoding::fBit3, CSnpBitfield::IEncoding::fBit4,
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1, CSnpBitfield::IEncoding::fBit2, CSnpBitfield::IEncoding::fBit3,
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1, CSnpBitfield::IEncoding::fBit2,
    -1,    CSnpBitfield::IEncoding::fBit0, -1   , CSnpBitfield::IEncoding::fBit1, -1   , CSnpBitfield::IEncoding::fBit2,
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1, CSnpBitfield::IEncoding::fBit2, CSnpBitfield::IEncoding::fBit3, CSnpBitfield::IEncoding::fBit4, CSnpBitfield::IEncoding::fBit5, CSnpBitfield::IEncoding::fBit6, CSnpBitfield::IEncoding::fBit7,
    CSnpBitfield::IEncoding::fBit0, CSnpBitfield::IEncoding::fBit1, -1   , -1   , CSnpBitfield::IEncoding::fBit2, CSnpBitfield::IEncoding::fBit3,
    CSnpBitfield::IEncoding::fBit2
};

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield2::CSnpBitfield2( const std::vector<char> &rhs )
{
    // TODO: use NCBI assertions?
    _ASSERT(rhs.size() == NUM_BYTES);

    std::vector<char>::const_iterator i_ci = rhs.begin();

    for(int i=0 ; i_ci != rhs.end(); ++i_ci, ++i) {
        m_listBytes[i] = *i_ci;
    }

    x_CreateString();
}

int CSnpBitfield2::GetVersion() const 
{
    return 2;
}

CSnpBitfield::EFunctionClass CSnpBitfield2::GetFunctionClass() const
{
    // On bytes 3 and 4

    unsigned char byte3 = m_listBytes[3];
    unsigned char byte4 = m_listBytes[4];

    // the 'Has reference' bit may be set.  So turn it off for now.    
    byte4 &= 0xfd; // 1111 1101 <- mask to set the 2nd bit to zero

    if (byte3 != 0 && byte4 != 0) 
        return CSnpBitfield::eMultipleFxn;

    switch ( byte3 )
    {
    case fBit0:     return  CSnpBitfield::eInGene;
    case fBit1:     return  CSnpBitfield::eInGene5;
    case fBit2:     return  CSnpBitfield::eInGene3;
    case fBit3:     return  CSnpBitfield::eIntron;
    case fBit4:     return  CSnpBitfield::eDonor;
    case fBit5:     return  CSnpBitfield::eAcceptor;
    case fBit6:     return  CSnpBitfield::eInUTR5;
    case fBit7:     return  CSnpBitfield::eInUTR3;
    }
    
    switch ( byte4 )
    {
    case fBit0:     return  CSnpBitfield::eSynonymous;
    case fBit2:     return  CSnpBitfield::eNonsense;
    case fBit3:     return  CSnpBitfield::eMissense;
    case fBit4:     return  CSnpBitfield::eFrameshift;
    }

    if ( m_listBytes[3]!=0 || byte4 != 0) {
        return CSnpBitfield::eMultipleFxn;
    }
    else {
        return CSnpBitfield::eUnknownFxn;
    }
}

CSnpBitfield::EVariationClass  CSnpBitfield2::GetVariationClass() const
{
    CSnpBitfield::EVariationClass c;
    unsigned char byte;

    byte = m_listBytes[10];
    if (byte <= CSnpBitfield::eMultiBase) // eMultiBase is last implemented variation
        c = (CSnpBitfield::EVariationClass) byte;
    else
        c = CSnpBitfield::eUnknownVariation;

    return c;
}

int  CSnpBitfield2::GetWeight() const
{
    const int mask   = 0x03;
    const int onByte = 5;
	return (m_listBytes[onByte] & mask);
}

bool CSnpBitfield2::IsTrue(CSnpBitfield::EProperty prop) const
{
    bool ret = false;

    // Special case for 'has reference'
    if (prop == CSnpBitfield::eHasReference) {
        return (m_listBytes[4] & 0x02);
    }

    // Return false if property queried is
    // newer than last property implemented at version 2 release
    // last property implemented was 'eHasShortReadArchive'
    if(prop > CSnpBitfield::eHasShortReadArchive) 
        return false;

    // perform table lookup to find byteoffset, and bitoffset
    int byteOffset  = g_byteOffset[prop];
    int bitMask     = g_bitOffset[prop];

    // check if property was removed (offsets will be -1)
    if(byteOffset == -1 || bitMask == -1)
        return false;

    ret = (m_listBytes[byteOffset] & bitMask) != 0;

    return ret;
}

bool CSnpBitfield2::IsTrue( CSnpBitfield::EFunctionClass prop ) const 
{
    bool ret = false;
    
    if (   prop == CSnpBitfield::eMultipleFxn 
        || prop == CSnpBitfield::eUnknownFxn ) {

        ret = ( GetFunctionClass() == prop );
    }
    else {        

        // looking for a specific function class
        unsigned char byte3 = m_listBytes[3];
        unsigned char byte4 = m_listBytes[4];
        
        // the 'Has reference' bit may be set.  So turn it off for test.
        byte4 &= 0xfd; // 1111 1101 <- mask to set the 2nd bit to zero

        switch (prop) {
            case  CSnpBitfield::eInGene:    ret = (byte3 & fBit0);  break;            
            case  CSnpBitfield::eInGene5:   ret = (byte3 & fBit1);  break;
            case  CSnpBitfield::eInGene3:   ret = (byte3 & fBit2);  break;
            case  CSnpBitfield::eIntron:    ret = (byte3 & fBit3);  break;
            case  CSnpBitfield::eDonor:     ret = (byte3 & fBit4);  break;
            case  CSnpBitfield::eAcceptor:  ret = (byte3 & fBit5);  break;
            case  CSnpBitfield::eInUTR5:    ret = (byte3 & fBit6);  break;
            case  CSnpBitfield::eInUTR3:    ret = (byte3 & fBit7);  break;

            // using byte4
            case  CSnpBitfield::eSynonymous:    ret = (byte4 & fBit0);  break;
            case  CSnpBitfield::eNonsense:      ret = (byte4 & fBit2);  break;
            case  CSnpBitfield::eMissense:      ret = (byte4 & fBit3);  break;
            case  CSnpBitfield::eFrameshift:    ret = (byte4 & fBit4);  break;

            // eUTR 
            case CSnpBitfield::eUTR:    ret = (byte3 & fBit6) || (byte3 & fBit7); break;

            default:
                ret = false;
        }
    }

    return ret;    
}

const char * CSnpBitfield2::GetString() const
{
    return m_strBits.c_str();
}

void CSnpBitfield2::x_CreateString()
{
    m_strBits.erase();

    char buff[5];
    for(unsigned int i=0; i < NUM_BYTES; i++) {
        unsigned char x = (unsigned char)m_listBytes[i];
        sprintf(buff, "%02hX", (unsigned int)x);
        m_strBits += buff;
        if(i+1 != NUM_BYTES)
            m_strBits += "-";
    }
}

CSnpBitfield::IEncoding * CSnpBitfield2::Clone()
{
    CSnpBitfield2 * obj = new CSnpBitfield2();

    memcpy(obj->m_listBytes, m_listBytes, sizeof(m_listBytes));
    obj->m_strBits = m_strBits;

    return obj;
}

END_NCBI_SCOPE
