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
 *  Provides implementation of CSnpBitfieldFactoryclass.
 *   See snp_bitfield_factory.hpp for class usage.
 *
 */

#include <ncbi_pch.hpp>

#include "snp_bitfield_factory.hpp"
#include "snp_bitfield_1_2.hpp"
#include "snp_bitfield_2.hpp"
#include "snp_bitfield_3.hpp"
#include "snp_bitfield_4.hpp"
#include "snp_bitfield_5.hpp"

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
//  Public Methods
///////////////////////////////////////////////////////////////////////////////
CSnpBitfield::IEncoding *
CSnpBitfieldFactory::CreateBitfield( vector<char> data )
{
    CSnpBitfield::IEncoding *bitfield = 0;

    // check size
    size_t size = data.size();

    // Only v1.2 and on are supported.  Previous versions were incorrectly
    //   encoded

    // For 1.2 version, there exists 10 bytes
    if (size == 10) {
        bitfield = new CSnpBitfield1_2(data);
    }
    // All other supported versions have at least 12 bytes and
    //  maintain a version number in least significant byte
    else if (size >=12) {
        unsigned char version = (unsigned char)data[0];
        switch(version) {
            case 2:     bitfield = new CSnpBitfield2(data);     break;
            case 3:     bitfield = new CSnpBitfield3(data);     break;
            case 4:     bitfield = new CSnpBitfield4(data);     break;
            case 5:     bitfield = new CSnpBitfield5(data);     break;

            // if version was undetected, 
            //   use last known version
            default:                
                bitfield = new CSnpBitfield5(data); 
                break;
        }
    }
    
    // if no bitfield created, create a null bitfield
    if (bitfield == 0) {
        bitfield = new CSnpBitfieldNull();
    }

    return bitfield;
}


///////////////////////////////////////////////////////////////////////////////
//  CSnpBitfieldNull - the Null Object of CSnpBitfield::IEncoding
///////////////////////////////////////////////////////////////////////////////
bool CSnpBitfieldNull::IsTrue( CSnpBitfield::EProperty e ) const
{
    return false;
}

bool CSnpBitfieldNull::IsTrue( CSnpBitfield::EFunctionClass e ) const
{
    return false;
}

int  CSnpBitfieldNull::GetWeight() const
{
    return 0;
}

int  CSnpBitfieldNull::GetVersion() const
{
    return 0;
}

CSnpBitfield::IEncoding * CSnpBitfieldNull::Clone()
{
    return new CSnpBitfieldNull();
}

CSnpBitfield::EFunctionClass CSnpBitfieldNull::GetFunctionClass() const
{
    return CSnpBitfield::eUnknownFxn;
}

CSnpBitfield::EVariationClass CSnpBitfieldNull::GetVariationClass() const
{
    return CSnpBitfield::eUnknownVariation;
}

const char * CSnpBitfieldNull::GetString() const
{
    return "";
}

END_NCBI_SCOPE
