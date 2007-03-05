#ifndef OBJECTS_OBJMGR_IMPL___HANDLE_RANGE_MAP__HPP
#define OBJECTS_OBJMGR_IMPL___HANDLE_RANGE_MAP__HPP

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
 *
 */

#include <objmgr/impl/handle_range.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_loc;


// Seq_loc substitution for internal use by iterators and data sources
class NCBI_XOBJMGR_EXPORT CHandleRangeMap
{
public:
    typedef CHandleRange::TRange TRange;
    typedef map<CSeq_id_Handle, CHandleRange> TLocMap;
    typedef TLocMap::const_iterator const_iterator;

    CHandleRangeMap(void);
    CHandleRangeMap(const CHandleRangeMap& rmap);
    ~CHandleRangeMap(void);

    CHandleRangeMap& operator= (const CHandleRangeMap& rmap);

    // Add all ranges for each seq-id from a seq-loc
    void AddLocation(const CSeq_loc& loc);
    void AddLocation(const CSeq_loc& loc, bool more_before, bool more_after);
    // Add range substituting with handle "h"
    void AddRange(const CSeq_id_Handle& h,
                  const TRange& range, ENa_strand strand);
    // Add ranges from "range" with handle "h"
    void AddRanges(const CSeq_id_Handle& h, const CHandleRange& hr);
    CHandleRange& AddRanges(const CSeq_id_Handle& h);

    // Get the ranges map
    const TLocMap& GetMap(void) const { return m_LocMap; }
    bool empty(void) const { return m_LocMap.empty(); }

    void clear(void);

    // iterate
    const_iterator begin(void) const { return m_LocMap.begin(); }
    const_iterator end(void) const { return m_LocMap.end(); }

    bool IntersectingWithLoc(const CSeq_loc& loc) const;
    bool IntersectingWithMap(const CHandleRangeMap& rmap) const;
    bool TotalRangeIntersectingWith(const CHandleRangeMap& rmap) const;

    void AddRange(const CSeq_id& id, TSeqPos from, TSeqPos to,
                  ENa_strand strand = eNa_strand_unknown);
    void AddRange(const CSeq_id& id,
                  const TRange& range, ENa_strand strand = eNa_strand_unknown);

    void AddRange(const CSeq_id_Handle& h,
                  const TRange& range, ENa_strand strand,
                  bool more_before, bool more_after);
    void AddRange(const CSeq_id& id,
                  const TRange& range, ENa_strand strand,
                  bool more_before, bool more_after);
    void AddRange(const CSeq_id& id,
                  TSeqPos from, TSeqPos to, ENa_strand strand,
                  bool more_before, bool more_after);

private:
    // Split the location and add range lists to the locmap
    void x_ProcessLocation(const CSeq_loc& loc);

    TLocMap m_LocMap;
};


inline
void CHandleRangeMap::AddRange(const CSeq_id_Handle& h,
                               const TRange& range, ENa_strand strand)
{
    AddRange(h, range, strand, false, false);
}


inline
void CHandleRangeMap::AddRange(const CSeq_id& id,
                               const TRange& range, ENa_strand strand)
{
    AddRange(id, range, strand, false, false);
}


inline
void CHandleRangeMap::AddRange(const CSeq_id& id,
                               TSeqPos from, TSeqPos to, ENa_strand strand,
                               bool more_before, bool more_after)
{
    AddRange(id, TRange(from, to), strand, more_before, more_after);
}


inline
void CHandleRangeMap::AddRange(const CSeq_id& id,
                               TSeqPos from, TSeqPos to, ENa_strand strand)
{
    AddRange(id, from, to, strand, false, false);
}


inline
void CHandleRangeMap::AddLocation(const CSeq_loc& loc)
{
    AddLocation(loc, false, false);
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR_IMPL___HANDLE_RANGE_MAP__HPP
