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
 * Authors:  Frank Ludwig
 *
 * File Description:  Exon number assignment for GTF records
 *
 */

#ifndef OBJTOOLS_WRITERS___EXON_NUMBER_ASSIGNMENT__HPP
#define OBJTOOLS_WRITERS___EXON_NUMBER_ASSIGNMENT__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/mapped_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGtfRecord;

//  ===========================================================================
class CExonNumberAssigner
//  ===========================================================================
{
public:
    CExonNumberAssigner(
        CMappedFeat mf) { xInitialize(mf); };

    bool CdsNeedsExonNumbers() const;

    unsigned int CdsGetExonNumberFor(
        const CGtfRecord&) const;

    void AssignExonNumberTo(
        CGtfRecord& gtfRecord) const;

private:
    void xInitialize(
        CMappedFeat cdsMf);

    unsigned xGetIndexInLocation(
        const CGtfRecord&,
        const CSeq_loc&) const;

    CMappedFeat mCdsMf;
    CSeq_loc mCdsLoc;
    CMappedFeat mRnaMf;
    CSeq_loc mRnaLoc;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___EXON_NUMBER_ASSIGNMENT__HPP
