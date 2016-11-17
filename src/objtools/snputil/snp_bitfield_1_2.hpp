#ifndef SNPUTIL___SNP_BITFIELD_1_2__HPP
#define SNPUTIL___SNP_BITFIELD_1_2__HPP

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
 *    CSnpBitfield1_2 represents the SNP Bitfield v1.2 format
 *    This is the first stable version of the format.  Previous versions
 *    were incorrectly encoded and should be disregarded.
 *    There are no earlier subclasses of CSnpBitfield::IEncoding before this
 *    class.
 */

#include <corelib/ncbistd.hpp>


#include <vector>
#include <objtools/snputil/snp_bitfield.hpp>

BEGIN_NCBI_SCOPE

/// Implement SNP Bitfield format v1.2
class CSnpBitfield1_2 : public CSnpBitfield::IEncoding
{

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:
    CSnpBitfield1_2 (const objects::CSeq_feat& feat);

    virtual bool    IsTrue( CSnpBitfield::EProperty prop )          const;
    virtual bool    IsTrue( CSnpBitfield::EFunctionClass prop )     const;

    virtual int                             GetWeight()             const;
    virtual int                             GetVersion()            const;
    virtual CSnpBitfield::EVariationClass   GetVariationClass()     const;
    virtual CSnpBitfield::EFunctionClass    GetFunctionClass()      const;

    virtual CSnpBitfield::IEncoding *       Clone();

private:
    CSnpBitfield1_2() {};


///////////////////////////////////////////////////////////////////////////////
// Private Data
///////////////////////////////////////////////////////////////////////////////
private:
    unsigned char   m_listBytes[10];
};

END_NCBI_SCOPE

#endif // SNPUTIL___SNP_BITFIELD_1_2__HPP


