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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/util/feature.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>

#include "exon_number_assignment.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ---------------------------------------------------------------------------
void CExonNumberAssigner::xInitialize(
    CMappedFeat cdsMf)
//  ---------------------------------------------------------------------------
{
    mCdsMf = cdsMf;
    mCdsLoc.Assign(cdsMf.GetLocation());
    mCdsLoc.ChangeToPackedInt();
    
    mRnaMf = feature::GetBestMrnaForCds(mCdsMf);
    if (mRnaMf) {
        mRnaLoc.Assign(mRnaMf.GetLocation());
        mRnaLoc.ChangeToPackedInt();
    }
    else {
        mRnaLoc.Reset();
    }
}

//  ---------------------------------------------------------------------------
bool CExonNumberAssigner::CdsNeedsExonNumbers() const
//  ---------------------------------------------------------------------------
{
    return true; //latest instruction

    //I suspect this will be back - or something close to it
    //if (!mRnaMf) {
    //    return (mCdsLoc.GetPacked_int().Get().size() > 1);
    //}
    //return (mRnaLoc.GetPacked_int().Get().size() > 1);
}

//  ---------------------------------------------------------------------------
unsigned int CExonNumberAssigner::xGetIndexInLocation(
    const CGtfRecord& gtfRecord,
    const CSeq_loc& location) const
//  ---------------------------------------------------------------------------
{
    _ASSERT(location.IsPacked_int());
    unsigned int exonNumber = 1;
    auto recordFrom = gtfRecord.SeqStart();
    auto recordTo = gtfRecord.SeqStop();
    if (recordFrom == 14408) {
        cerr << "";
    }
    for (const auto& pExonLocation: location.GetPacked_int().Get()) {
        auto exonFrom = pExonLocation->GetFrom();
        auto exonTo = pExonLocation->GetTo();
        if (recordFrom >= exonFrom  &&  recordTo <= exonTo) {
            return exonNumber;
        }
        ++exonNumber;
    }
    return 0; // invalid exon number
}

//  ---------------------------------------------------------------------------
unsigned int CExonNumberAssigner::CdsGetExonNumberFor(
    const CGtfRecord& gtfRecord) const
//  ---------------------------------------------------------------------------
{
    if (!mRnaMf) {
        return xGetIndexInLocation(gtfRecord, mCdsLoc);
    }
    return xGetIndexInLocation(gtfRecord, mRnaLoc);
}

//  ----------------------------------------------------------------------------
void CExonNumberAssigner::AssignExonNumberTo(
    CGtfRecord& gtfRecord) const
//  ----------------------------------------------------------------------------
{
    auto exonNumber = CdsGetExonNumberFor(gtfRecord);
    _ASSERT(exonNumber); // indicator of faulty assignment logic
    gtfRecord.SetExonNumber(exonNumber); 
}

    
END_objects_SCOPE
END_NCBI_SCOPE
