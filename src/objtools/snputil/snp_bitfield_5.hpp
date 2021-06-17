#ifndef SNPUTIL___SNP_BITFIELD_5__HPP
#define SNPUTIL___SNP_BITFIELD_5__HPP

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
 *      CSnpBitfield5 implements the SNP Bitfield v5 format.  It is
 *        related to v4 format and so the implementation
 *        inherits from CSnpBitfield4.  Additional fields were added,
 *        some were redefined, and this class will try to be future-compatible
 *        with newer bitfield formats.
 *
 */

#include <corelib/ncbistd.hpp>

#include <vector>
#include <objtools/snputil/snp_bitfield.hpp>

#include "snp_bitfield_4.hpp"

BEGIN_NCBI_SCOPE

/// Implement SNP Bitfield format v5
class CSnpBitfield5 : public CSnpBitfield4
{

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:
    CSnpBitfield5(const objects::CSeq_feat& feat);

    virtual bool    IsTrue( CSnpBitfield::EProperty prop )          const;
    virtual bool    IsTrue( CSnpBitfield::EFunctionClass prop )     const;
    virtual CSnpBitfield::EFunctionClass    GetFunctionClass()      const;
    virtual int                             GetVersion()            const;
    virtual CSnpBitfield::IEncoding *       Clone();

protected:
    CSnpBitfield5() {};

///////////////////////////////////////////////////////////////////////////////
// Private Data
///////////////////////////////////////////////////////////////////////////////
private:

};

END_NCBI_SCOPE

#endif // GUI_WIDGETS_SNP___SNP_BITFIELD_5__HPP


