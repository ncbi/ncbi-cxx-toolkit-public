#ifndef ALGO_SEQ__ANNOT_COMPARE__HPP
#define ALGO_SEQ__ANNOT_COMPARE__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMappedFeat;
class CSeq_feat;
class CScope;
class CSeq_loc;

class NCBI_XALGOSEQ_EXPORT CAnnotCompare
{
public:
    enum ECompareFlags {
        ///
        /// genomic location categories
        ///

        /// match not found
        eLocation_Missing               = 0,

        /// no difference in lcoation
        eLocation_Same                  = 0x01,

        /// missing an exon, either 5', 3', or internal
        eLocation_MissingExon           = 0x02,

        /// 5', 3' extended exon
        eLocation_5PrimeExtension       = 0x03,
        eLocation_3PrimeExtension       = 0x04,

        /// 5', 3' extra exons
        eLocation_5PrimeExtraExon       = 0x05,
        eLocation_3PrimeExtraExon       = 0x06,

        /// locations overlap without sharing exon boundaries
        eLocation_Overlap               = 0x07,

        /// one location is a subset of another
        eLocation_Subset                = 0x08,

        /// locations exhibit a non-shared-exon overlap of total range
        eLocation_RegionOverlap         = 0x09,

        /// locations annotated on different strands
        eLocation_DifferentStrand       = 0x0a,

        /// 

        /// complex relation - results may be returned as a set of the
        /// above
        eLocation_Complex               = 0x0b,

        ///
        /// sequence categories (genomic and product)
        ///

        /// locations have the same sequence
        eSequence_SameSeq               = 0x0100,

        /// locations have a different sequence
        eSequence_DifferentSeq          = 0x0200,

        /// locations have the same product sequence
        eSequence_SameProduct           = 0x0400,

        /// locations have a different product sequence
        eSequence_DifferentProduct      = 0x0800,

        /// masks for the above
        eLocationMask = 0xff,
        eSequenceMask = 0xff00
    };
    typedef int TCompareFlags;

    ///@name Feature compariosns
    ///@{

    /// compare two different features, possibly living in different scopes.
    /// This variant takes raw seq-feats and performs the comparisons as needed.
    TCompareFlags CompareFeats(const CSeq_feat& feat1,
                               CScope&          scope1,
                               const CSeq_feat& feat2,
                               CScope&          scope2,
                               vector<ECompareFlags>* complex_flags,
                               list<string>* comments);

    /// compare two different features, possibly living in different scopes.
    /// This variant takes CMappedFeat objects (returned by CFeat_CI)
    /// and performs the comparisons as needed.
    TCompareFlags CompareFeats(const CMappedFeat& feat1,
                               CScope&            scope1,
                               const CMappedFeat& feat2,
                               CScope&            scope2,
                               vector<ECompareFlags>* complex_flags,
                               list<string>* comments);

    /// compare two different features, possibly living in different scopes.
    /// This variant takes a pair of CSeq_feats and mapped locations.
    TCompareFlags CompareFeats(const CSeq_feat& feat1,
                               const CSeq_loc&  mapped_loc1,
                               CScope&          scope1,
                               const CSeq_feat& feat2,
                               const CSeq_loc&  mapped_loc2,
                               CScope&          scope2,
                               vector<ECompareFlags>* complex_flags,
                               list<string>* comments);

    ///@}

private:

};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_SEQ__ANNOT_COMPARE__HPP
