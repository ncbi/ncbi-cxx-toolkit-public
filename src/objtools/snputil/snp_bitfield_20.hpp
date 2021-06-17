#ifndef SNPUTIL___SNP_BITFIELD_20__HPP
#define SNPUTIL___SNP_BITFIELD_20__HPP

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
 * Authors:  Dmitry Rudnev
 *
 * File Description:
 *      CSnpBitfield20 implements the SNP 2.0 (VDB) 64-bit Bitfield format.
 *      https://confluence.ncbi.nlm.nih.gov/display/VAR/dbSNP2.0+8-byte+%2864-bit%29+Bitfield
 *      for comparison with previous bitfield (96-bit), see https://confluence.ncbi.nlm.nih.gov/display/VAR/dbSNP+12-byte+%2896%29+Bitfield
 */

#include <corelib/ncbistd.hpp>

#include <vector>
#include <objtools/snputil/snp_bitfield.hpp>
#include <objtools/variation/SnpBitAttributes.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

BEGIN_NCBI_SCOPE

class CSnpBitfield20 : public CSnpBitfield::IEncoding
{

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:
    CSnpBitfield20(const objects::CSeq_feat& feat);

    virtual bool    IsTrue( CSnpBitfield::EProperty prop )          const;
    virtual bool    IsTrue( CSnpBitfield::EFunctionClass prop )     const;

    // all weights in SNP 2.0 are assumed to be 1
    virtual int                             GetWeight()              const { return 1; }
    virtual int                             GetVersion()             const { return 20; }
    virtual CSnpBitfield::EFunctionClass    GetFunctionClass()       const;
    virtual CSnpBitfield::EVariationClass   GetVariationClass()      const { return m_VariationClass; }
    virtual CSnpBitfield::IEncoding *       Clone();

private:
    CSnpBitfield20() {}

///////////////////////////////////////////////////////////////////////////////
// Private Data
///////////////////////////////////////////////////////////////////////////////
private:
    // internal representation for the SNP 2.0 bitfield
    unique_ptr<NDbSnp::CSnpBitAttributes> m_bits;

    // variation class is not stored in the bitfield anymore, so we extract it from the feature and store it separately
    CSnpBitfield::EVariationClass m_VariationClass{CSnpBitfield::eUnknownVariation};

};

END_NCBI_SCOPE

#endif // GUI_WIDGETS_SNP___SNP_BITFIELD_20__HPP


