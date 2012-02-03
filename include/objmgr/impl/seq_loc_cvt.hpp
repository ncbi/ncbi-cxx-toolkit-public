#ifndef SEQ_LOC_CVT__HPP
#define SEQ_LOC_CVT__HPP

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
*   Object manager iterators
*
*/

#include <corelib/ncbiobj.hpp>

#include <util/range.hpp>
#include <util/rangemap.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>
#include <objmgr/impl/heap_scope.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqMap_CI;
class CScope;
class CSeq_align_Mapper;
class CAnnotObject_Ref;

class CSeq_id;
class CSeq_loc;
class CSeq_interval;
class CSeq_point;
class CInt_fuzz;

class CSeq_feat;
class CSeq_align;
class CDense_seg;
class CPacked_seg;
class CSeq_align_set;
struct SAnnotObject_Index;


/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CSeq_loc_Conversion : public CObject
{
public:
    typedef CRange<TSeqPos> TRange;

    // Create conversion based on a seq-map segment
    CSeq_loc_Conversion(CSeq_loc&             master_loc_empty,
                        const CSeq_id_Handle& dst_id,
                        const CSeqMap_CI&     seg,
                        const CSeq_id_Handle& src_id,
                        CScope*               scope);
    // Create conversion based on ranges and IDs
    CSeq_loc_Conversion(CSeq_loc&             master_loc_empty,
                        const CSeq_id_Handle& dst_id,
                        const TRange&         dst_rg,
                        const CSeq_id_Handle& src_id,
                        TSeqPos               src_start,
                        bool                  reverse,
                        CScope*               scope);

    ~CSeq_loc_Conversion(void);

    // Add mapping from current destination through one more conversion
    // The new destination becomes the one of cvt, range may be truncated.
    void CombineWith(CSeq_loc_Conversion& cvt);

    TSeqPos ConvertPos(TSeqPos src_pos);

    bool GoodSrcId(const CSeq_id& id);
    bool MinusStrand(void) const
        {
            return m_Reverse;
        }

    void ConvertSimpleLoc(const CSeq_id_Handle& src_id,
                          const CRange<TSeqPos> src_range,
                          const SAnnotObject_Index& src_index);
    bool ConvertPoint(TSeqPos src_pos, ENa_strand src_strand);
    bool ConvertPoint(const CSeq_point& src);

    bool ConvertInterval(TSeqPos src_from, TSeqPos src_to,
                         ENa_strand src_strand);
    bool ConvertInterval(const CSeq_interval& src);

    void ConvertFeature(CAnnotObject_Ref& ref,
                        const CSeq_feat& orig_feat,
                        CRef<CSeq_feat>& mapped_feat);
    void ConvertCdregion(CAnnotObject_Ref& ref,
                         const CSeq_feat& orig_feat,
                         CRef<CSeq_feat>& mapped_feat);
    void ConvertRna(CAnnotObject_Ref& ref,
                    const CSeq_feat& orig_feat,
                    CRef<CSeq_feat>& mapped_feat);

    CConstRef<CInt_fuzz> ReverseFuzz(const CInt_fuzz& fuzz) const;

    enum EConvertFlag {
        eCnvDefault,
        eCnvAlways
    };
    enum ELocationType {
        eLocation,
        eProduct
    };

    bool Convert(const CSeq_loc& src, CRef<CSeq_loc>* dst,
                 EConvertFlag flag = eCnvDefault);

    void Reset(void);

    bool IsPartial(void) const
        {
            return m_Partial;
        }

    void SetSrcId(const CSeq_id_Handle& src)
        {
            m_Src_id_Handle = src;
        }
    void SetConversion(const CSeqMap_CI& seg);

    const CSeq_id_Handle& GetSrc_id_Handle(void) const
        {
            return m_Src_id_Handle;
        }
    TSeqPos GetSrc_from(void) const
        {
            return m_Src_from;
        }
    TSeqPos GetSrc_to(void) const
        {
            return m_Src_to;
        }

    const TRange& GetTotalRange(void) const
        {
            return m_TotalRange;
        }

    ENa_strand ConvertStrand(ENa_strand strand) const;

    void SetMappedLocation(CAnnotObject_Ref& ref, ELocationType loctype);
    void MakeDstMix(CSeq_loc_mix& dst, const CSeq_loc_mix& src) const;

    const CSeq_id& GetId(void) const
        {
            return m_Dst_loc_Empty->GetEmpty();
        }

protected:
    friend class CAnnot_Collector;

    void Convert(CAnnotObject_Ref& obj,
                 ELocationType loctype);
    void Convert(CAnnotObject_Ref& ref,
                 ELocationType loctype,
                 const CSeq_id_Handle& id,
                 const CRange<TSeqPos>& range,
                 const SAnnotObject_Index& index);

private:
    void CheckDstInterval(void);
    void CheckDstPoint(void);
    void CheckDstMix(void);

    CRef<CSeq_interval> GetDstInterval(void);
    CRef<CSeq_point> GetDstPoint(void);
    CRef<CSeq_loc_mix> GetDstMix(void);

    void SetDstLoc(CRef<CSeq_loc>* loc);

    bool IsSpecialLoc(void) const;

    CSeq_loc& GetDstLocEmpty(void)
        {
            return *m_Dst_loc_Empty;
        }
    CSeq_id& GetDstId(void)
        {
            return m_Dst_loc_Empty->SetEmpty();
        }

    TRange GetDstRange(void)
        {
            return m_Reverse ?
                TRange(ConvertPos(m_Src_to), ConvertPos(m_Src_from)) :
                TRange(ConvertPos(m_Src_from), ConvertPos(m_Src_to));
        }
    TRange GetSrcRange(void) const
        {
            return TRange(m_Src_from, m_Src_to);
        }

    void ConvertPacked_int(const CSeq_loc& src, CRef<CSeq_loc>* dst);
    void ConvertPacked_pnt(const CSeq_loc& src, CRef<CSeq_loc>* dst);
    bool ConvertSimpleMix(const CSeq_loc& src);
    void ConvertMix(const CSeq_loc& src, CRef<CSeq_loc>* dst,
                    EConvertFlag flag = eCnvDefault);
    void ConvertEquiv(const CSeq_loc& src, CRef<CSeq_loc>* dst);
    void ConvertBond(const CSeq_loc& src, CRef<CSeq_loc>* dst);

    // Translation parameters:
    //   Source id and bounds:
    CSeq_id_Handle m_Src_id_Handle;
    TSeqPos        m_Src_from;
    TSeqPos        m_Src_to;

    //   Source to destination shift:
    TSignedSeqPos  m_Shift;
    bool           m_Reverse;

    //   Destination id:
    CSeq_id_Handle m_Dst_id_Handle;
    CRef<CSeq_loc> m_Dst_loc_Empty;

    // Results:
    //   Cumulative results on destination:
    TRange         m_TotalRange;
    bool           m_Partial;

    // Separate flags for left and right truncations of each interval
    enum EPartialFlag {
        fPartial_from = 1 << 0, // the interval is partial on the left
        fPartial_to   = 1 << 1  // the interval is partial on the right
    };
    typedef int TPartialFlag;

    TPartialFlag m_PartialFlag;
    CConstRef<CInt_fuzz> m_DstFuzz_from;
    CConstRef<CInt_fuzz> m_DstFuzz_to;
    
    //   Last Point, Interval or other simple location's conversion result:
    enum EMappedObjectType {
        eMappedObjType_not_set,
        eMappedObjType_Seq_loc,
        eMappedObjType_Seq_point,
        eMappedObjType_Seq_interval,
        eMappedObjType_Seq_loc_mix
    };
    EMappedObjectType m_LastType;
    TRange         m_LastRange;
    ENa_strand     m_LastStrand;
    CConstRef<CSeq_loc> m_SrcLoc;

    // Scope for id resolution:
    CHeapScope     m_Scope;

    CRef<CGraphRanges> m_GraphRanges;

    friend class CSeq_loc_Conversion_Set;
    friend class CSeq_align_Mapper;
    friend struct CConversionRef_Less;
};


class NCBI_XOBJMGR_EXPORT CSeq_loc_Conversion_Set : public CObject
{
public:
    CSeq_loc_Conversion_Set(CHeapScope& scope);

    typedef CRange<TSeqPos> TRange;
    typedef CRangeMultimap<CRef<CSeq_loc_Conversion>, TSeqPos> TRangeMap;
    typedef TRangeMap::iterator TRangeIterator;
    typedef map<CSeq_id_Handle, TRangeMap> TIdMap;

    // Conversions by location index
    typedef map<unsigned int, TIdMap> TConvByIndex;

    void Add(CSeq_loc_Conversion& cvt, unsigned int loc_index);
    TRangeIterator BeginRanges(CSeq_id_Handle id,
                               TSeqPos from,
                               TSeqPos to,
                               unsigned int loc_index);
    void Convert(CAnnotObject_Ref& obj,
                 CSeq_loc_Conversion::ELocationType loctype);
    bool Convert(const CSeq_loc& src,
                 CRef<CSeq_loc>* dst,
                 unsigned int loc_index);
    void Convert(const CSeq_align& src, CRef<CSeq_align>* dst);

private:
    friend class CSeq_align_Mapper;

    void x_Add(CSeq_loc_Conversion& cvt, unsigned int loc_index);

    bool ConvertPoint(const CSeq_point& src,
                      CRef<CSeq_loc>* dst,
                      unsigned int loc_index);
    bool ConvertInterval(const CSeq_interval& src,
                         CRef<CSeq_loc>* dst,
                         unsigned int loc_index);

    bool ConvertPacked_int(const CSeq_loc& src,
                           CRef<CSeq_loc>* dst,
                           unsigned int loc_index);
    bool ConvertPacked_pnt(const CSeq_loc& src,
                           CRef<CSeq_loc>* dst,
                           unsigned int loc_index);
    bool ConvertMix(const CSeq_loc& src,
                    CRef<CSeq_loc>* dst,
                    unsigned int loc_index);
    bool ConvertEquiv(const CSeq_loc& src,
                      CRef<CSeq_loc>* dst,
                      unsigned int loc_index);
    bool ConvertBond(const CSeq_loc& src,
                     CRef<CSeq_loc>* dst,
                     unsigned int loc_index);
    void ConvertFeature(CAnnotObject_Ref& ref,
                        const CSeq_feat& orig_feat,
                        CRef<CSeq_feat>& mapped_feat);
    void ConvertCdregion(CAnnotObject_Ref& ref,
                         const CSeq_feat& orig_feat,
                         CRef<CSeq_feat>& mapped_feat);
    void ConvertRna(CAnnotObject_Ref& ref,
                    const CSeq_feat& orig_feat,
                    CRef<CSeq_feat>& mapped_feat);

    CRef<CSeq_loc_Conversion> m_SingleConv;
    unsigned int              m_SingleIndex;
    TConvByIndex m_CvtByIndex;
    bool         m_Partial;
    TRange       m_TotalRange;
    CHeapScope   m_Scope;

    CRef<CGraphRanges> m_GraphRanges;
};


inline
bool CSeq_loc_Conversion::IsSpecialLoc(void) const
{
    return m_LastType >= eMappedObjType_Seq_point;
}


inline
TSeqPos CSeq_loc_Conversion::ConvertPos(TSeqPos src_pos)
{
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return kInvalidSeqPos;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        dst_pos = m_Shift + src_pos;
    }
    else {
        dst_pos = m_Shift - src_pos;
    }
    return dst_pos;
}


inline
bool CSeq_loc_Conversion::GoodSrcId(const CSeq_id& id)
{
    bool good = (m_Src_id_Handle == id);
    if ( !good ) {
        m_Partial = true;
    }
    return good;
}


inline
ENa_strand CSeq_loc_Conversion::ConvertStrand(ENa_strand strand) const
{
    if ( m_Reverse ) {
        strand = Reverse(strand);
    }
    return strand;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_TYPES_CI__HPP
