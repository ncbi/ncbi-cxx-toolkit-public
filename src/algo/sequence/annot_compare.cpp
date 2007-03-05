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

#include <ncbi_pch.hpp>
#include <algo/sequence/annot_compare.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CAnnotCompare::TCompareFlags
CAnnotCompare::CompareFeats(const CSeq_feat& feat1,
                            CScope& scope1,
                            const CSeq_feat& feat2,
                            CScope& scope2,
                            vector<ECompareFlags>* complex_flags,
                            list<string>* comments)
{
    return CompareFeats(feat1, feat1.GetLocation(), scope1,
                        feat2, feat2.GetLocation(), scope2,
                        complex_flags, comments);
}


CAnnotCompare::TCompareFlags
CAnnotCompare::CompareFeats(const CMappedFeat& feat1,
                            CScope& scope1,
                            const CMappedFeat& feat2,
                            CScope& scope2,
                            vector<ECompareFlags>* complex_flags,
                            list<string>* comments)
{
    return CompareFeats(feat1.GetOriginalFeature(),
                        feat1.GetLocation(), scope1,
                        feat2.GetOriginalFeature(),
                        feat2.GetLocation(), scope2,
                        complex_flags, comments);
}


///
/// this version is the real work-horse
///

CAnnotCompare::TCompareFlags
CAnnotCompare::CompareFeats(const CSeq_feat& feat1,
                            const CSeq_loc& loc1,
                            CScope& scope1,
                            const CSeq_feat& feat2,
                            const CSeq_loc& loc2,
                            CScope& scope2,
                            vector<ECompareFlags>* complex_flags,
                            list<string>* comments)
{
    TCompareFlags loc_state = 0;

    ENa_strand strand1 = sequence::GetStrand(loc1, &scope1);
    ENa_strand strand2 = sequence::GetStrand(loc2, &scope2);
    if (!SameOrientation(strand1, strand2)) {
        loc_state |= eLocation_Missing;
    } else {
        sequence::ECompare comp_val = sequence::Compare(loc1, loc2, &scope1);
        switch (comp_val) {
        case sequence::eSame:
            loc_state |= eLocation_Same;
            break;

        case sequence::eOverlap:
            loc_state |= eLocation_Complex;
            break;

        case sequence::eContains:
        case sequence::eContained:
            {{
                CSeq_loc_CI loc1_iter(loc1);
                size_t loc1_exons = 0;
                for ( ;  loc1_iter;  ++loc1_iter, ++loc1_exons) {
                }
                CSeq_loc_CI loc2_iter(loc2);
                size_t loc2_exons = 0;
                for ( ;  loc2_iter;  ++loc2_iter, ++loc2_exons) {
                }
                bool rev = IsReverse(strand1);
                TSeqRange range1 = loc1.GetTotalRange();
                TSeqRange range2 = loc2.GetTotalRange();
                if (loc1_exons == loc2_exons) {
                    bool agreement_3prime;
                    bool agreement_5prime;
                    if (!rev) {
                        agreement_5prime =
                            range1.GetFrom() == range2.GetFrom();
                        agreement_3prime =
                            range1.GetTo() == range2.GetTo();
                    } else {
                        agreement_3prime =
                            range1.GetFrom() == range2.GetFrom();
                        agreement_5prime =
                            range1.GetTo() == range2.GetTo();
                    }

                    loc1_iter.Rewind();
                    loc2_iter.Rewind();
                    bool agreement_internal = true;
                    for (unsigned int i = 0;
                         i < loc1_exons;
                         ++i, ++loc1_iter, ++loc2_iter) {

                        if ((i != 0 || rev)
                            && (i != loc1_exons - 1 || !rev)) {
                            if (loc1_iter.GetRange().GetFrom()
                                != loc2_iter.GetRange().GetFrom()) {
                                agreement_internal = false;
                                break;
                            }
                        }

                        if ((i != 0 || !rev)
                            && (i != loc1_exons - 1 || rev)) {
                            if (loc1_iter.GetRange().GetTo()
                                != loc2_iter.GetRange().GetTo()) {
                                agreement_internal = false;
                                break;
                            }
                        }

                    }

                    if (!agreement_internal) {
                        loc_state |= eLocation_Complex;
                    } else if (agreement_5prime && !agreement_3prime) {
                        loc_state |= eLocation_3PrimeExtension;
                    } else if (agreement_3prime && !agreement_5prime) {
                        loc_state |= eLocation_5PrimeExtension;
                    } else {
                        // both 3' and 5' disagreement
                        loc_state |= eLocation_Complex;
                    }
                } else {
                    loc1_iter.Rewind();
                    loc2_iter.Rewind();
                    while (loc1_iter && loc2_iter) {
                        if (loc1_iter.GetRange() == loc2_iter.GetRange()) {
                            ++loc1_iter;
                            ++loc2_iter;
                        } else {
                            if (loc1_exons > loc2_exons) {
                                ++loc1_iter;
                            } else {
                                ++loc2_iter;
                            }
                        }
                    }
                    if ((loc1_exons > loc2_exons && !loc2_iter) ||
                        (loc2_exons > loc1_exons && !loc1_iter)) {
                        loc_state |= eLocation_MissingExon;
                    } else {
                        loc_state |= eLocation_Complex;
                    }
                }
            }}
            break;

        default:
        case sequence::eNoOverlap:
            loc_state |= eLocation_Missing;
            break;
        }
    }

    ///
    /// now, do a very simple sequence comparison
    ///
    CSeqVector v1(loc1, scope1);
    CSeqVector v2(loc2, scope2);
    CSeqVector_CI v1_iter = v1.begin();
    CSeqVector_CI v2_iter = v2.begin();
    TCompareFlags seq_state = 0;
    for (size_t count = 0;
         v1_iter != v1.end()  &&  v2_iter != v2.end();
         ++v1_iter, ++v2_iter, ++count) {
        if (*v1_iter != *v2_iter) {
            seq_state |= eSequence_DifferentSeq;
            break;
        }
    }
    if (v1_iter != v1.end()  ||  v2_iter != v2.end()) {
        seq_state |= eSequence_DifferentSeq;
    }
    if (seq_state) {
        loc_state |= seq_state;
    } else {
        loc_state |= eSequence_SameSeq;
    }

    ///
    /// also compare products
    ///
    if (feat1.IsSetProduct()  &&  feat2.IsSetProduct()) {
        CSeqVector v1(feat1.GetProduct(), scope1);
        CSeqVector v2(feat2.GetProduct(), scope2);
        CSeqVector_CI v1_iter = v1.begin();
        CSeqVector_CI v2_iter = v2.begin();
        for ( ;  v1_iter != v1.end()  &&  v2_iter != v2.end();  ++v1_iter, ++v2_iter) {
            if (*v1_iter != *v2_iter) {
                loc_state |= eSequence_DifferentProduct;
                break;
            }
        }
        if ((loc_state & eSequence_DifferentProduct) == 0) {
            loc_state |= eSequence_SameProduct;
        }
    } else if (feat1.IsSetProduct() != feat2.IsSetProduct()) {
        loc_state |= eSequence_DifferentProduct;
    }

    return loc_state;
}


END_NCBI_SCOPE
