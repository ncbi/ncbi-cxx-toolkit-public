#ifndef OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP
#define OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- location representation
*
*/

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declarations
class CInt_fuzz;
class CSeq_id;
class CSeq_interval;
class CSeq_loc;
class CFFContext;


class CFlatSeqLoc : public CObject // derived from CObject to allow for caching
{
public:
    struct SInterval
    {
        enum EFlags {
            fReversed     = 0x1,
            fPartialLeft  = 0x2,
            fPartialRight = 0x4
        };
        typedef int TFlags; // binary OR of EFlags

        TSeqPos GetStart(void) const
          { return m_Flags & fReversed ? m_Range.GetTo() : m_Range.GetFrom(); }
        TSeqPos GetStop(void) const
          { return m_Flags & fReversed ? m_Range.GetFrom() : m_Range.GetTo(); }

        bool IsReversed    (void) const
            { return (m_Flags & fReversed) != 0; }
        bool IsPartialLeft (void) const
            { return (m_Flags & fPartialLeft) != 0; }
        bool IsPartialRight(void) const
            { return (m_Flags & fPartialRight) != 0; }

        string          m_Accession;
        CRange<TSeqPos> m_Range; // 1-based, L->R; should be finite
        TFlags          m_Flags;
    };
    typedef vector<SInterval> TIntervals;
    
    CFlatSeqLoc(const CSeq_loc& loc, CFFContext& ctx);

    const string&     GetString(void)    const { return m_String;    }
    const TIntervals& GetIntervals(void) const { return m_Intervals; }
    
private:
    string     m_String;    // whole location, as a GB-style string
    TIntervals m_Intervals; // individual intervals/points

    void x_Add   (const CSeq_loc& loc, CNcbiOstrstream& oss,
                  CFFContext& ctx);
    void x_Add   (const CSeq_interval& si, CNcbiOstrstream& oss,
                  CFFContext& ctx);
    // these convert from 0-based to 1-based coordinates in the process
    SInterval::TFlags x_AddPnt(TSeqPos pnt, const CInt_fuzz* fuzz,
                               CNcbiOstrstream& oss, CFFContext& ctx);
    void x_AddInt(TSeqPos from, TSeqPos to, const string& accn,
                  SInterval::TFlags flags = 0);

    static void x_AddID(const CSeq_id& id, CNcbiOstrstream& oss,
                        CFFContext& ctx, string *s = 0);
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/12/17 19:47:33  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP */
