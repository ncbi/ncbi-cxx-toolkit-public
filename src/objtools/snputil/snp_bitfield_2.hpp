#ifndef SNPUTIL___SNP_BITFIELD_2__HPP
#define SNPUTIL___SNP_BITFIELD_2__HPP

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
 *    CSnpBitfield2 represents the SNP Bitfield v2 format.
 *    The format introduces a versioning number stored in the lowest order
 *    byte.  It also adds more gene function properties and removes
 *    previously defined hapmap properties (e.g. 'phase [1-3] attempted')
 *
 */

#include <objtools/snputil/snp_bitfield.hpp>

BEGIN_NCBI_SCOPE

/// Implement SNP Bitfield format v2
class CSnpBitfield2 : public CSnpBitfield::IEncoding
{

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:
    CSnpBitfield2 (const std::vector<char> &rhs);

    virtual bool    IsTrue( CSnpBitfield::EProperty prop )          const;
    virtual bool    IsTrue( CSnpBitfield::EFunctionClass prop )     const;

    virtual int                             GetWeight()             const;
    virtual int                             GetVersion()            const;
    virtual CSnpBitfield::EVariationClass   GetVariationClass()     const;
    virtual CSnpBitfield::EFunctionClass    GetFunctionClass()      const;
    virtual const char *                    GetString()             const;

    virtual CSnpBitfield::IEncoding *       Clone();

protected:
    enum EBit
    {
        BIT_1 = 0x01,
        BIT_2 = 0x02,
        BIT_3 = 0x04,
        BIT_4 = 0x08,
        BIT_5 = 0x10,
        BIT_6 = 0x20,
        BIT_7 = 0x40,
        BIT_8 = 0x80
    };

///////////////////////////////////////////////////////////////////////////////
// Protected Methods
///////////////////////////////////////////////////////////////////////////////
protected:
    CSnpBitfield2() {};
    void x_CreateString();


///////////////////////////////////////////////////////////////////////////////
// Protected Data
///////////////////////////////////////////////////////////////////////////////
protected:
    unsigned char   m_listBytes[12];
    std::string     m_strBits;

};

END_NCBI_SCOPE

#endif // SNPUTIL___SNP_BITFIELD_2__HPP


