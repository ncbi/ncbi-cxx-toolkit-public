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
 * File Description:  Generate stable GFF3 IDs
 *
 */

#ifndef OBJTOOLS_READERS___GFF3_IDGEN__HPP
#define OBJTOOLS_READERS___GFF3_IDGEN__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/writers/write_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  =============================================================================
class NCBI_XOBJWRITE_EXPORT CGffIdGenerator
    //  =============================================================================
{
public:
    typedef enum {
        fNormal = 0,
    } TFlags;

public:
    CGffIdGenerator(
        TFlags flags = fNormal): 
        mFlags(flags),
        mLastTrulyGenericSuffix(0)
    {};
    ~CGffIdGenerator() {};

    std::string GetGffId(
        const CMappedFeat&,
        CGffFeatureContext& fc);

    std::string GetGffId();

    std::string GetGffSourceId(
        CBioseq_Handle);

    std::string GetNextGffExonId(
        const std::string&);

    void Reset();

protected:
    std::string xGetIdForGene(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xGetIdForRna(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xGetIdForCds(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xGetGenericId(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xGetGenericSuffix(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xExtractGeneLocusTagOrLocus(
        const CMappedFeat&);
    
    std::string xExtractLocalId(
        const CMappedFeat&);

    std::string xExtractTrackingId(
        const CMappedFeat&);

    std::string xExtractFarAccession(
        const CMappedFeat&);

    std::string xExtractFeatureLocation(
        const CMappedFeat&,
        CGffFeatureContext&);

    std::string xDisambiguate(
        const std::string&);

protected:
    TFlags mFlags;
    std::set<std::string> mExistingIds;
    std::map<std::string, int> mLastUsedExonIds;
    unsigned int mLastTrulyGenericSuffix;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_IDGEN__HPP
