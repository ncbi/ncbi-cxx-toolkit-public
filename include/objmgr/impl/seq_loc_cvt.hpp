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

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqMap_CI;
class CScope;
class CAnnotObject_Ref;

class CSeq_id;
class CSeq_loc;
class CSeq_interval;
class CSeq_point;

/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion
/////////////////////////////////////////////////////////////////////////////

class CSeq_loc_Conversion
{
public:
    CSeq_loc_Conversion(const CSeq_id& master_id,
                        const CSeqMap_CI& seg,
                        const CSeq_id_Handle& src_id,
                        CScope* scope);
    ~CSeq_loc_Conversion(void);

    TSeqPos ConvertPos(TSeqPos src_pos);

    bool GoodSrcId(const CSeq_id& id);
    bool MinusStrand(void) const
        {
            return m_Reverse;
        }

    bool ConvertPoint(TSeqPos src_pos);
    bool ConvertPoint(const CSeq_point& src);

    bool ConvertInterval(TSeqPos src_from, TSeqPos src_to);
    bool ConvertInterval(const CSeq_interval& src);

    bool Convert(const CSeq_loc& src, CRef<CSeq_loc>& dst,
                 bool always = false);

    void Convert(CAnnotObject_Ref& obj, int index);

    void Reset(void);

    bool IsPartial(void) const
        {
            return m_Partial;
        }

    const CSeq_id& GetDstId(void) const
        {
            return m_Dst_id;
        }
    CSeq_loc* GetDstLocWhole(void);

    void SetSrcId(const CSeq_id_Handle& src)
        {
            m_Src_id = src;
        }
    void SetConversion(const CSeqMap_CI& seg);

    typedef CRange<TSeqPos> TRange;
    const TRange& GetTotalRange(void) const
        {
            return m_TotalRange;
        }
    
    ENa_strand ConvertStrand(ENa_strand strand) const;

private:
    CSeq_id_Handle m_Src_id;
    TSeqPos        m_Src_from;
    TSeqPos        m_Src_to;
    TSignedSeqPos  m_Shift;
    const CSeq_id& m_Dst_id;
    TRange         m_TotalRange;
    bool           m_Reverse;
    bool           m_Partial;
    CScope*        m_Scope;
    // temporaries for conversion results
    CRef<CSeq_interval> dst_int;
    CRef<CSeq_point>    dst_pnt;
    CRef<CSeq_loc> m_DstLocWhole;
};


inline
void CSeq_loc_Conversion::Reset(void)
{
    m_TotalRange = TRange::GetEmpty();
    m_Partial = false;
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
    bool good = m_Scope->GetIdHandle(id) == m_Src_id;
    if ( !good ) {
        m_Partial = true;
    }
    return good;
}


inline
ENa_strand CSeq_loc_Conversion::ConvertStrand(ENa_strand strand) const
{
    if ( m_Reverse ) {
        switch ( strand ) {
        case eNa_strand_minus:
            strand = eNa_strand_plus;
            break;
        case eNa_strand_plus:
            strand = eNa_strand_minus;
            break;
        case eNa_strand_both:
            strand = eNa_strand_both_rev;
            break;
        case eNa_strand_both_rev:
            strand = eNa_strand_both;
            break;
        }
    }
    return strand;
}


inline
bool CSeq_loc_Conversion::ConvertPoint(const CSeq_point& src)
{
    if ( !GoodSrcId(src.GetId()) || !ConvertPoint(src.GetPoint()) ) {
        return false;
    }
    if ( src.IsSetStrand() ) {
        dst_pnt->SetStrand(ConvertStrand(src.GetStrand()));
    }
    return true;
}


inline
bool CSeq_loc_Conversion::ConvertInterval(const CSeq_interval& src)
{
    if ( !GoodSrcId(src.GetId()) ||
         !ConvertInterval(src.GetFrom(), src.GetTo()) ) {
        return false;
    }
    if ( src.IsSetStrand() ) {
        dst_int->SetStrand(ConvertStrand(src.GetStrand()));
    }
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* ===========================================================================
*/

#endif  // ANNOT_TYPES_CI__HPP
