#ifndef SEQLOC_CI__HPP
#define SEQLOC_CI__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-loc ranges iterator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/04/16 20:01:17  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Forward declarations
class CSeq_loc;
class CSeq_id;


class CSeq_loc_CI
{
public:
    CSeq_loc_CI(void);
    CSeq_loc_CI(const CSeq_loc& loc);
    virtual ~CSeq_loc_CI(void);

    CSeq_loc_CI(const CSeq_loc_CI& iter);
    CSeq_loc_CI& operator= (const CSeq_loc_CI& iter);

    CSeq_loc_CI& operator++ (void);
    CSeq_loc_CI& operator++ (int);
    operator bool (void) const;

    typedef CRange<int> TRange;

    // Get seq_id of the current location
    const CSeq_id& GetSeq_id(void) const;
    // Get the range
    // Get starting point
    TRange         GetRange(void) const;
    // Get strand
    ENa_strand GetStrand(void) const;

    // True if the current location is a whole sequence
    bool           IsWhole(void) const;
    // True if the current location is empty
    bool           IsEmpty(void) const;
    // True if the current location is a single point
    bool           IsPoint(void) const;

private:
    // Check the iterator position
    bool x_IsValid(void) const;
    // Check the position, throw runtime_error if not valid
    void x_ThrowNotValid(string where) const;

    // Process the location, fill the list
    void x_ProcessLocation(const CSeq_loc& loc);

    // Simple location structure: id/from/to
    struct SLoc_Info {
        SLoc_Info(void);
        SLoc_Info(const SLoc_Info& loc_info);
        SLoc_Info& operator= (const SLoc_Info& loc_info);

        CConstRef<CSeq_id> m_Id;
        TRange             m_Range;
        ENa_strand         m_Strand;
    };

    typedef list<SLoc_Info> TLocList;

    // Prevent seq-loc destruction
    CConstRef<CSeq_loc>      m_Location;
    // List of intervals
    TLocList                 m_LocList;
    // Current interval
    TLocList::const_iterator m_CurLoc;
};



inline
CSeq_loc_CI::SLoc_Info::SLoc_Info(void)
    : m_Id(0), m_Strand(eNa_strand_unknown)
{
    return;
}

inline
CSeq_loc_CI::SLoc_Info::SLoc_Info(const SLoc_Info& loc_info)
    : m_Id(loc_info.m_Id),
      m_Range(loc_info.m_Range),
      m_Strand(loc_info.m_Strand)
{
    return;
}

inline
CSeq_loc_CI::SLoc_Info&
CSeq_loc_CI::SLoc_Info::operator= (const SLoc_Info& loc_info)
{
    if (this == &loc_info)
        return *this;
    m_Id = loc_info.m_Id;
    m_Range = loc_info.m_Range;
    m_Strand = loc_info.m_Strand;
    return *this;
}

inline
CSeq_loc_CI& CSeq_loc_CI::operator++ (void)
{
    m_CurLoc++;
    return *this;
}

inline
CSeq_loc_CI& CSeq_loc_CI::operator++ (int)
{
    m_CurLoc++;
    return *this;
}

inline
CSeq_loc_CI::operator bool (void) const
{
    return x_IsValid();
}

inline
const CSeq_id& CSeq_loc_CI::GetSeq_id(void) const
{
    x_ThrowNotValid("GetSeq_id()");
    return *m_CurLoc->m_Id;
}

inline
CSeq_loc_CI::TRange CSeq_loc_CI::GetRange(void) const
{
    x_ThrowNotValid("GetRange()");
    return m_CurLoc->m_Range;
}

inline
ENa_strand CSeq_loc_CI::GetStrand(void) const
{
    x_ThrowNotValid("GetRange()");
    return m_CurLoc->m_Strand;
}

inline
bool CSeq_loc_CI::IsWhole(void) const
{
    x_ThrowNotValid("IsWhole()");
    return m_CurLoc->m_Range.IsWholeFrom()  &&
        m_CurLoc->m_Range.IsWholeTo();
}

inline
bool CSeq_loc_CI::IsEmpty(void) const
{
    x_ThrowNotValid("IsEmpty()");
    return m_CurLoc->m_Range.IsEmptyFrom()  &&
        m_CurLoc->m_Range.IsEmptyTo();
}

inline
bool CSeq_loc_CI::IsPoint(void) const
{
    x_ThrowNotValid("IsPoint()");
    return m_CurLoc->m_Range.GetLength() == 1;
}

inline
bool CSeq_loc_CI::x_IsValid(void) const
{
    return m_CurLoc != m_LocList.end();
}

inline
void CSeq_loc_CI::x_ThrowNotValid(string where) const
{
    if ( x_IsValid() )
        return;
    throw runtime_error("CSeq_loc_CI::" + where + " -- iterator is not valid");
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQLOC_CI__HPP
