#ifndef OBJECTS_OBJMGR___SEQ_MAP_SWITCH__HPP
#define OBJECTS_OBJMGR___SEQ_MAP_SWITCH__HPP

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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*   Working with seq-map switch points
*
*/

#include <objmgr/seq_map.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <util/range.hpp>
#include <vector>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqMap;
class CBioseq_Handle;
class CSeq_align;

/** @addtogroup ObjectManagerSequenceRep
 *
 * @{
 */

class NCBI_XOBJMGR_EXPORT CSeqMapSwitchPoint : public CObject
{
public:

    // master sequence
    CBioseq_Handle  m_Master;
    // point in master sequence - coordinate of the first base to the right
    TSeqPos         m_MasterPos;

    // first encountered alignment used to calculate this switch point
    CConstRef<CSeq_align> m_FirstAlign;

    // point on the segment to the left of current switch point
    CSeq_id_Handle  m_LeftId;
    TSeqPos         m_LeftPos;
    bool            m_LeftMinusStrand;

    // point on the segment to the right of current switch point
    CSeq_id_Handle  m_RightId;
    TSeqPos         m_RightPos;
    bool            m_RightMinusStrand;

    // range of possible positions of the switch point
    CRange<TSeqPos> m_MasterRange;
    // range of possible positions of the switch point with exact segment match
    CRange<TSeqPos> m_ExactMasterRange;

    // information about changes in sequence
    //   first - insertions
    //   second - deletions
    typedef pair<TSeqPos, TSeqPos> TInsertDelete;
    // key is offset from the current switch point
    typedef map<TSeqPos, TInsertDelete> TDifferences;
    TDifferences m_LeftDifferences, m_RightDifferences;

    // all possible changes
    TInsertDelete GetDifferences(TSeqPos new_pos, TSeqPos add = 0) const;
    // possible change in sequence length
    int GetLengthDifference(TSeqPos new_pos, TSeqPos add = 0) const
        {
            TInsertDelete diff = GetDifferences(new_pos, add);
            return diff.first - diff.second;
        }
    // possible change in sequence bases (sum of insertions and deletions)
    int GetSequenceDifference(TSeqPos new_pos, TSeqPos add = 0) const
        {
            TInsertDelete diff = GetDifferences(new_pos, add);
            return diff.first + diff.second;
        }

    TSeqPos GetInsert(TSeqPos pos) const;
    TSeqPos GetLeftInPlaceInsert(void) const;
    TSeqPos GetRightInPlaceInsert(void) const;

    void ChangeSwitchPoint(TSeqPos pos, TSeqPos add);
    void InsertInPlace(TSeqPos add_left, TSeqPos add_right);

    bool operator<(const CSeqMapSwitchPoint& p) const
        {
            _ASSERT(m_Master == p.m_Master);
            return m_MasterPos < p.m_MasterPos;
        }
};

/* @} */

// calculate switch point for two segments specified by align
NCBI_XOBJMGR_EXPORT
CRef<CSeqMapSwitchPoint> GetSwitchPoint(const CBioseq_Handle& seq,
                                        const CSeq_align& align);

typedef vector<CRef<CSeqMapSwitchPoint> > TSeqMapSwitchPoints;
typedef list<CRef<CSeq_align> > TSeqMapSwitchAligns;

// calculate all sequence switch points using set of Seq-aligns
NCBI_XOBJMGR_EXPORT
TSeqMapSwitchPoints GetAllSwitchPoints(const CBioseq_Handle& seq,
                                       const TSeqMapSwitchAligns& aligns);

// calculate all sequence switch points using set of Seq-aligns from assembly
NCBI_XOBJMGR_EXPORT
TSeqMapSwitchPoints GetAllSwitchPoints(const CBioseq_Handle& seq);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJECTS_OBJMGR___SEQ_MAP_SWITCH__HPP
