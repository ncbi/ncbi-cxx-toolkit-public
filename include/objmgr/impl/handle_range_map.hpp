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
    typedef map<CSeq_id_Handle, CHandleRange> TLocMap;
    typedef TLocMap::const_iterator const_iterator;

    CHandleRangeMap(void);
    CHandleRangeMap(const CHandleRangeMap& rmap);
    ~CHandleRangeMap(void);

    CHandleRangeMap& operator= (const CHandleRangeMap& rmap);

    // Add all ranges for each seq-id from a seq-loc
    void AddLocation(const CSeq_loc& loc);
    // Add range substituting with handle "h"
    void AddRange(const CSeq_id_Handle& h,
                  CHandleRange::TRange range, ENa_strand strand);
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
    void AddRange(const CSeq_id& id, CHandleRange::TRange range,
                  ENa_strand strand = eNa_strand_unknown);

private:
    // Split the location and add range lists to the locmap
    void x_ProcessLocation(const CSeq_loc& loc);

    TLocMap m_LocMap;
};


inline
void CHandleRangeMap::AddRange(const CSeq_id& id,
                               TSeqPos from, TSeqPos to,
                               ENa_strand strand)
{
    AddRange(id, CHandleRange::TRange(from, to), strand);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.16  2004/07/12 15:05:31  grichenk
 * Moved seq-id mapper from xobjmgr to seq library
 *
 * Revision 1.15  2003/07/17 20:07:55  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.14  2003/06/02 16:01:37  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.13  2003/04/24 16:12:37  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.12  2003/02/25 14:48:07  vasilche
 * Added Win32 export modifier to object manager classes.
 *
 * Revision 1.11  2003/02/24 18:57:21  vasilche
 * Make feature gathering in one linear pass using CSeqMap iterator.
 * Do not use feture index by sub locations.
 * Sort features at the end of gathering in one vector.
 * Extracted some internal structures and classes in separate header.
 * Delay creation of mapped features.
 *
 * Revision 1.10  2003/02/05 17:57:41  dicuccio
 * Moved into include/objects/objmgr/impl to permit data loaders to be defined
 * outside of xobjmgr.
 *
 * Revision 1.9  2003/01/29 22:03:46  grichenk
 * Use single static CSeq_id_Mapper instead of per-OM model.
 *
 * Revision 1.8  2003/01/22 20:11:54  vasilche
 * Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
 * CSeqMap_CI now supports resolution and iteration over sequence range.
 * Added several caches to CScope.
 * Optimized CSeqVector().
 * Added serveral variants of CBioseqHandle::GetSeqVector().
 * Tried to optimize annotations iterator (not much success).
 * Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
 *
 * Revision 1.7  2002/12/06 15:36:00  grichenk
 * Added overlap type for annot-iterators
 *
 * Revision 1.6  2002/07/08 20:51:01  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.5  2002/06/12 14:40:47  grichenk
 * Made some methods inline
 *
 * Revision 1.4  2002/02/21 19:27:06  grichenk
 * Rearranged includes. Added scope history. Added searching for the
 * best seq-id match in data sources and scopes. Updated tests.
 *
 * Revision 1.3  2002/02/15 20:35:38  gouriano
 * changed implementation of HandleRangeMap
 *
 * Revision 1.2  2002/01/23 21:59:31  grichenk
 * Redesigned seq-id handles and mapper
 *
 * Revision 1.1  2002/01/11 19:06:20  gouriano
 * restructured objmgr
 *
 *
 * ===========================================================================
 */

#endif  // OBJECTS_OBJMGR_IMPL___HANDLE_RANGE_MAP__HPP
