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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotations selector structure.
*
*/


#include <corelib/ncbi_limits.h>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;
class CSeq_entry;
class CTSE_Info;

struct NCBI_XOBJMGR_EXPORT SAnnotTypeSelector
{
    typedef CSeq_annot::C_Data::E_Choice TAnnotChoice;
    typedef CSeqFeatData::E_Choice       TFeatChoice;

    SAnnotTypeSelector(TAnnotChoice annot = CSeq_annot::C_Data::e_not_set,
                       TFeatChoice  feat  = CSeqFeatData::e_not_set,
                       int feat_product = false)
        : m_AnnotChoice(annot),
          m_FeatChoice(feat),
          m_FeatProduct(feat_product)
    {
    }

    SAnnotTypeSelector(TFeatChoice  feat,
                   int feat_product = false)
        : m_AnnotChoice(CSeq_annot::C_Data::e_Ftable),
          m_FeatChoice(feat),
          m_FeatProduct(feat_product)
    {
    }
   
    TAnnotChoice GetAnnotChoice(void) const
        {
            return m_AnnotChoice;
        }

    TFeatChoice GetFeatChoice(void) const
        {
            return m_FeatChoice;
        }

    int GetFeatProduct(void) const
        {
            return m_FeatProduct;
        }

    bool operator<(const SAnnotTypeSelector& s) const
        {
            return m_AnnotChoice < s.m_AnnotChoice ||
                m_AnnotChoice == s.m_AnnotChoice &&
                (m_FeatChoice < s.m_FeatChoice ||
                 m_FeatChoice == s.m_FeatChoice &&
                 (m_FeatProduct < s.m_FeatProduct));
        }

    TAnnotChoice          m_AnnotChoice;  // Annotation type
    TFeatChoice           m_FeatChoice;   // Seq-feat subtype
    int                   m_FeatProduct;  // "true" for searching products
};

// Structure to select type of Seq-annot
struct NCBI_XOBJMGR_EXPORT SAnnotSelector : public SAnnotTypeSelector
{
    typedef CSeqFeatData::ESubtype       TFeatSubtype;

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
    // Flag to indicate adaptive segment selection
    enum ESegmentSelect {
        eSegmentSelect_All,
        eSegmentSelect_First,
        eSegmentSelect_Last
    };
    // Flag to indicate sorting method
    enum ESortOrder {
        eSortOrder_None,    // do not sort annotations for faster retrieval
        eSortOrder_Normal,  // default - increasing start, decreasing length
        eSortOrder_Reverse  // decresing end, decreasing length
    };
    // Flag to indicate joining feature visible through multiple segments
    enum ECombineMethod {
        eCombine_None,       // default - do not combine feature from segments
        eCombine_All         // combine feature from different segments
    };
    enum ELimitObject {
        eLimit_None,
        eLimit_TSE,
        eLimit_Entry,
        eLimit_Annot
    };
    enum EIdResolving {
        eLoadedOnly,       // Resolve only ids already loaded into the scope
        eIgnoreUnresolved, // Ignore unresolved ids (default)
        eFailUnresolved    // Throw exception for unresolved ids
    };

    SAnnotSelector(TAnnotChoice annot = CSeq_annot::C_Data::e_not_set,
                   TFeatChoice  feat  = CSeqFeatData::e_not_set,
                   int feat_product = false)
        : SAnnotTypeSelector(annot, feat, feat_product),
          m_ResolveDepth(kMax_Int),
          m_FeatSubtype(CSeqFeatData::eSubtype_any),
          m_OverlapType(eOverlap_Intervals),
          m_ResolveMethod(eResolve_TSE),
          m_SegmentSelect(eSegmentSelect_All),
          m_SortOrder(eSortOrder_Normal),
          m_CombineMethod(eCombine_None),
          m_LimitObjectType(eLimit_None),
          m_IdResolving(eIgnoreUnresolved),
          m_MaxSize(0),
          m_NoMapping(false),
          m_AdaptiveDepth(false)
    {
    }

    SAnnotSelector(TFeatChoice  feat,
                   int feat_product = false)
        : SAnnotTypeSelector(CSeq_annot::C_Data::e_Ftable, feat, feat_product),
          m_ResolveDepth(kMax_Int),
          m_FeatSubtype(CSeqFeatData::eSubtype_any),
          m_OverlapType(eOverlap_Intervals),
          m_ResolveMethod(eResolve_TSE),
          m_SegmentSelect(eSegmentSelect_All),
          m_SortOrder(eSortOrder_Normal),
          m_CombineMethod(eCombine_None),
          m_LimitObjectType(eLimit_None),
          m_IdResolving(eIgnoreUnresolved),
          m_MaxSize(0),
          m_NoMapping(false),
          m_AdaptiveDepth(false)
    {
    }
    
    SAnnotSelector& SetAnnotChoice(TAnnotChoice choice)
        {
            m_AnnotChoice = choice;
            // Reset feature type/subtype
            if (m_AnnotChoice != CSeq_annot::C_Data::e_Ftable) {
                m_FeatChoice = CSeqFeatData::e_not_set;
                m_FeatSubtype = CSeqFeatData::eSubtype_any;
            }
            return *this;
        }
    SAnnotSelector& SetFeatChoice(TFeatChoice choice)
        {
            m_FeatChoice = choice;
            // Adjust annot type and feature subtype
            m_AnnotChoice = CSeq_annot::C_Data::e_Ftable;
            m_FeatSubtype = CSeqFeatData::eSubtype_any;
            return *this;
        }

    SAnnotSelector& SetFeatSubtype(TFeatSubtype subtype)
        {
            m_FeatSubtype = subtype;
            // Adjust annot type and feature type
            m_AnnotChoice = CSeq_annot::C_Data::e_Ftable;
            if (m_FeatSubtype != CSeqFeatData::eSubtype_any) {
                m_FeatChoice =
                    CSeqFeatData::GetTypeFromSubtype(m_FeatSubtype);
            }
            return *this;
        }
    TFeatSubtype GetFeatSubtype(void) const
        {
            return m_FeatSubtype;
        }

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
    SAnnotSelector& SetResolveDepth(int depth)
        {
            m_ResolveDepth = depth;
            return *this;
        }
    SAnnotSelector& SetAdaptiveDepth(bool value)
        {
            m_AdaptiveDepth = value;
            return *this;
        }
    SAnnotSelector& SetMaxSize(int max_size)
        {
            m_MaxSize = max_size;
            return *this;
        }

    SAnnotSelector& SetSegmentSelect(ESegmentSelect ss)
        {
            m_SegmentSelect = ss;
            return *this;
        }
    SAnnotSelector& SetSegmentSelectAll(void)
        {
            return SetSegmentSelect(eSegmentSelect_All);
        }
    SAnnotSelector& SetSegmentSelectFirst(void)
        {
            return SetSegmentSelect(eSegmentSelect_First);
        }
    SAnnotSelector& SetSegmentSelectLast(void)
        {
            return SetSegmentSelect(eSegmentSelect_Last);
        }

    SAnnotSelector& SetLimitNone(void);
    SAnnotSelector& SetLimitTSE(const CSeq_entry* tse);
    SAnnotSelector& SetLimitSeqEntry(const CSeq_entry* entry);
    SAnnotSelector& SetLimitSeqAnnot(const CSeq_annot* annot);

    SAnnotSelector& SetIdResolvingLoaded(void)
        {
            m_IdResolving = eLoadedOnly;
            return *this;
        }
    SAnnotSelector& SetIdResolvingIgnore(void)
        {
            m_IdResolving = eIgnoreUnresolved;
            return *this;
        }
    SAnnotSelector& SetIdResolvingFail(void)
        {
            m_IdResolving = eFailUnresolved;
            return *this;
        }

    struct NCBI_XOBJMGR_EXPORT SSources : public CObject
    {
    public:
        SSources(void);
        ~SSources(void);
        
        typedef set<string> TSources;

        TSources m_Sources;

    private:
        SSources(const SSources&);
        const SSources& operator=(const SSources&);
    };

    SAnnotSelector& SetDataSource(const string& source);
    bool IsSetDataSources(void) const
        {
            return m_Sources;
        }
    bool HasDataSource(const string& source) const
        {
            return bool(m_Sources) &&
                m_Sources->m_Sources.find(source) !=
                m_Sources->m_Sources.end();
        }

    // No locations mapping flag. Set to true by CAnnot_CI.
    SAnnotSelector& SetNoMapping(bool value)
        {
            m_NoMapping = value;
            return *this;
        }

protected:
    int                   m_ResolveDepth;
    TFeatSubtype          m_FeatSubtype;  // Seq-feat subtype
    EOverlapType          m_OverlapType;
    EResolveMethod        m_ResolveMethod;
    ESegmentSelect        m_SegmentSelect;
    ESortOrder            m_SortOrder;
    ECombineMethod        m_CombineMethod;
    ELimitObject          m_LimitObjectType;
    EIdResolving          m_IdResolving;
    CConstRef<CObject>    m_LimitObject;
    int                   m_MaxSize;
    CRef<SSources>        m_Sources;
    bool                  m_NoMapping;
    bool                  m_AdaptiveDepth;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2003/09/16 14:21:46  grichenk
* Added feature indexing and searching by subtype.
*
* Revision 1.16  2003/09/11 17:45:06  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.15  2003/08/22 14:58:55  grichenk
* Added NoMapping flag (to be used by CAnnot_CI for faster fetching).
* Added GetScope().
*
* Revision 1.14  2003/07/17 21:01:43  vasilche
* Fixed ambiguity on Windows
*
* Revision 1.13  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.12  2003/06/25 20:56:29  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.11  2003/06/17 20:34:02  grichenk
* Added flag to ignore sorting
*
* Revision 1.10  2003/04/28 14:59:58  vasilche
* Added missing initialization.
*
* Revision 1.9  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.8  2003/03/31 21:48:17  grichenk
* Added possibility to select feature subtype through SAnnotSelector.
*
* Revision 1.7  2003/03/18 14:46:35  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.6  2003/03/14 20:39:26  ucko
* return *this from SetIdResolvingXxx().
*
* Revision 1.5  2003/03/14 19:10:33  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.4  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
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
