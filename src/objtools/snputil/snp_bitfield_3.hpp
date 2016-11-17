#ifndef SNPUTIL___SNP_BITFIELD_3__HPP
#define SNPUTIL___SNP_BITFIELD_3__HPP

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
 *      CSnpBitfield3 implements the SNP Bitfield v3 file format.  It is
 *        closely related to v2 file format and so the implementation
 *        borrows from CSnpBitfield2.  The changes between v3 and v2
 *        is the addition of eContigAlleleAbsent (byte 11, bit 4), and
 *        the replacement of eHasMPO with eHasClinicalAssay (same position)
 *
 */

#include <corelib/ncbistd.hpp>

#include <vector>
#include <objtools/snputil/snp_bitfield.hpp>

#include "snp_bitfield_2.hpp"

BEGIN_NCBI_SCOPE

/// Implement SNP Bitfield format v3 (nearly identical to v2)
class CSnpBitfield3 : public CSnpBitfield2
{

///////////////////////////////////////////////////////////////////////////////
// Public Methods
///////////////////////////////////////////////////////////////////////////////
public:
    CSnpBitfield3 (const objects::CSeq_feat& feat);

    virtual bool    IsTrue( CSnpBitfield::EProperty prop )          const;
    virtual int                             GetVersion()            const;
    virtual CSnpBitfield::IEncoding *       Clone();

protected:
    CSnpBitfield3() {};

///////////////////////////////////////////////////////////////////////////////
// Private Data
///////////////////////////////////////////////////////////////////////////////
private:

};

END_NCBI_SCOPE

#endif // SNPUTIL___SNP_BITFIELD_3__HPP


