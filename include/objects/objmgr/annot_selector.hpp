#ifndef ANNOT_SELECTOR__HPP
#define ANNOT_SELECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Annotations selector structure.
*
*/


#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/objmgr/impl/tse_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Structure to select type of Seq-annot
struct NCBI_XOBJMGR_EXPORT SAnnotSelector
{
    typedef CSeq_annot::C_Data::E_Choice TAnnotChoice;
    typedef CSeqFeatData::E_Choice       TFeatChoice;

    SAnnotSelector(TAnnotChoice annot = CSeq_annot::C_Data::e_not_set,
                   TFeatChoice  feat  = CSeqFeatData::e_not_set,
                   int feat_product = false)
        : m_AnnotChoice(annot),
          m_FeatChoice(feat),
          m_FeatProduct(feat_product),
          m_OverlapType(eOverlap_Intervals),
          m_ResolveMethod(eResolve_TSE),
          m_SortOrder(eSortOrder_Normal),
          m_CombineMethod(eCombine_None)
    {
    }

    SAnnotSelector(TFeatChoice  feat,
                   int feat_product = false)
        : m_AnnotChoice(CSeq_annot::C_Data::e_Ftable),
          m_FeatChoice(feat),
          m_FeatProduct(feat_product),
          m_OverlapType(eOverlap_Intervals),
          m_ResolveMethod(eResolve_TSE),
          m_SortOrder(eSortOrder_Normal),
          m_CombineMethod(eCombine_None)
    {
    }
    

/*
    bool operator==(const SAnnotSelector& sel) const
        {
            return
                m_AnnotChoice == sel.m_AnnotChoice &&
                m_FeatChoice == sel.m_FeatChoice &&
                m_FeatProduct == sel.m_FeatProduct;
        }
    bool operator< (const SAnnotSelector& sel) const
        {
            return
                m_AnnotChoice < sel.m_AnnotChoice ||
                (m_AnnotChoice == sel.m_AnnotChoice &&
                 (m_FeatChoice < sel.m_FeatChoice ||
                  (m_FeatChoice == sel.m_FeatChoice &&
                   m_FeatProduct < sel.m_FeatProduct)));
        }
*/

    TAnnotChoice m_AnnotChoice;  // Annotation type
    TFeatChoice  m_FeatChoice;   // Seq-feat subtype
    int          m_FeatProduct;  // set to "true" for searching products

    SAnnotSelector& SetAnnotChoice(TAnnotChoice choice)
        {
            m_AnnotChoice = choice;
            return *this;
        }
    

    // Flag to indicate location overlapping method
    enum EOverlapType {
        eOverlap_Intervals,  // default - overlapping of individual intervals
        eOverlap_TotalRange  // overlapping of total ranges only
    };
    // Flag to indicate references resolution method
    enum EResolveMethod {
        eResolve_None, // Do not search annotations on segments
        eResolve_TSE,  // default - search only on segments in the same TSE
        eResolve_All   // Search annotations for all referenced sequences
    };
    // Flag to indicate sorting method
    enum ESortOrder {
        eSortOrder_Normal,  // default - increasing start, decreasing length
        eSortOrder_Reverse  // decresing end, decreasing length
    };
    // Flag to indicate joining feature visible through multiple segments
    enum ECombineMethod {
        eCombine_None,       // default - do not combine feature from segments
        eCombine_All         // combine feature from different segments
    };

    SAnnotSelector& SetByProduct(bool byProduct = true)
        {
            m_FeatProduct = byProduct;
            return *this;
        }
    SAnnotSelector& SetOverlapType(EOverlapType overlap_type)
        {
            m_OverlapType = overlap_type;
            return *this;
        }
    SAnnotSelector& SetOverlapIntervals(void)
        {
            return SetOverlapType(eOverlap_Intervals);
        }
    SAnnotSelector& SetOverlapTotalRange(void)
        {
            return SetOverlapType(eOverlap_TotalRange);
        }
    SAnnotSelector& SetSortOrder(ESortOrder sort_order)
        {
            m_SortOrder = sort_order;
            return *this;
        }
    SAnnotSelector& SetCombineMethod(ECombineMethod combine_method)
        {
            m_CombineMethod = combine_method;
            return *this;
        }
    SAnnotSelector& SetResolveMethod(EResolveMethod resolve_method)
        {
            m_ResolveMethod = resolve_method;
            return *this;
        }
    SAnnotSelector& SetResolveNone(void)
        {
            return SetResolveMethod(eResolve_None);
        }
    SAnnotSelector& SetResolveTSE(void)
        {
            return SetResolveMethod(eResolve_TSE);
        }
    SAnnotSelector& SetResolveAll(void)
        {
            return SetResolveMethod(eResolve_All);
        }
    SAnnotSelector& SetLimitTSE(CTSE_Info* tse)
        {
            m_LimitTSE.Set(tse);
            return *this;
        }
    SAnnotSelector& SetLimitSeqEntry(const CSeq_entry* entry)
        {
            m_LimitSeqEntry.Reset(entry);
            return *this;
        }
    SAnnotSelector& SetLimitSeqAnnot(const CSeq_annot* annot)
        {
            m_LimitSeqAnnot.Reset(annot);
            return *this;
        }

    EOverlapType          m_OverlapType;
    EResolveMethod        m_ResolveMethod;
    ESortOrder            m_SortOrder;
    ECombineMethod        m_CombineMethod;
    CTSE_Lock             m_LimitTSE;
    CConstRef<CSeq_entry> m_LimitSeqEntry;
    CConstRef<CSeq_annot> m_LimitSeqAnnot;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.2  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.1  2003/02/24 19:36:19  vasilche
* Added missing annot_selector.hpp.
*
*
* ===========================================================================
*/

#endif  // ANNOT_SELECTOR__HPP
