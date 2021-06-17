#ifndef OBJTOOLS___SNP_BITFIELD_FACTORY__HPP
#define OBJTOOLS___SNP_BITFIELD_FACTORY__HPP
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
 *  Provides declaration of CSnpBitfieldFactory and CSnpBitfieldNull class.
 *  Do not use directly. Use CSnpBitfield instead.
 *
 */

#include <vector>

#include <objtools/snputil/snp_bitfield.hpp>

BEGIN_NCBI_SCOPE

class CSnpBitfieldNull;

// Creates the proper bitfield encoding based on the data
//  Intended to be used only by CSnpBitfield facade
class CSnpBitfieldFactory
{
public:
    static CSnpBitfield::IEncoding * CreateBitfield(const objects::CSeq_feat& feat);
};


// Null Object pattern for CSnpBitfield::IEncoding
class CSnpBitfieldNull : public CSnpBitfield::IEncoding
{
    virtual bool                            IsTrue(CSnpBitfield::EProperty e) const;
    virtual bool                            IsTrue(CSnpBitfield::EFunctionClass e) const;
    virtual int                             GetWeight() const;
    virtual int                             GetVersion() const;
    virtual CSnpBitfield::EFunctionClass    GetFunctionClass() const;
    virtual CSnpBitfield::EVariationClass   GetVariationClass() const;
    virtual CSnpBitfield::IEncoding *       Clone();
};


END_NCBI_SCOPE
#endif // OBJTOOLS___SNP_BITFIELD_FACTORY__HPP


